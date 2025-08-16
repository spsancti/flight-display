// ESP32 ADS-B Flight Display (Proof of Concept)
// - Connects to Wi‑Fi
// - Calls adsb.lol /v2/point/{lat}/{lon}/{radius}
// - Parses nearest aircraft and renders summary on SSD1322 OLED

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include <SPI.h>
#if defined(ESP32)
#include <esp_system.h>
#endif
#include <WebServer.h>

#include "config.h"  // Create from config.example.h and do not commit secrets
#include "aircraft_types.h"

// Forward declaration to satisfy Arduino's auto-generated prototypes
struct FlightInfo;

// Relay module pins and logic (override in config.h if desired)
#ifndef RELAY_IN1_PIN
#define RELAY_IN1_PIN 25  // Power/Status (blink on boot, solid after first display)
#endif
#ifndef RELAY_IN2_PIN
#define RELAY_IN2_PIN 26  // COM
#endif
#ifndef RELAY_IN3_PIN
#define RELAY_IN3_PIN 27  // PVT
#endif
#ifndef RELAY_IN4_PIN
#define RELAY_IN4_PIN 33  // MIL
#endif
#ifndef RELAY_ACTIVE_HIGH
#define RELAY_ACTIVE_HIGH 0  // Default to active-LOW modules; set to 1 if active-HIGH
#endif
// Blink IN1 during boot? Relays draw significant current; default OFF to avoid brownouts.
#ifndef RELAY_BLINK_ON_BOOT
#define RELAY_BLINK_ON_BOOT 0
#endif

// Boot staging: allow power to settle before Wi‑Fi/TLS ramps current
#ifndef BOOT_POWER_SETTLE_MS
#define BOOT_POWER_SETTLE_MS 1200
#endif

// Use lower TX power during connection to reduce inrush; bump after got IP
#ifndef WIFI_BOOT_TXPOWER
#define WIFI_BOOT_TXPOWER WIFI_POWER_8_5dBm
#endif
#ifndef WIFI_RUN_TXPOWER
#define WIFI_RUN_TXPOWER WIFI_POWER_15dBm
#endif

static inline void relayWrite(int pin, bool on) {
  if (pin < 0) return; // allow disabling a channel by setting pin to -1
  digitalWrite((uint8_t)pin, (RELAY_ACTIVE_HIGH ? (on ? HIGH : LOW) : (on ? LOW : HIGH)));
}

static void updateRelaysForState(bool haveDisplayed, const String &opClass, bool blinkPhase) {
  if (!haveDisplayed) {
    // Keep all relays OFF during boot to avoid brownouts from coil inrush.
    // Optionally blink IN1 if explicitly enabled.
    relayWrite(RELAY_IN1_PIN, (RELAY_BLINK_ON_BOOT ? blinkPhase : false));
    relayWrite(RELAY_IN2_PIN, false);
    relayWrite(RELAY_IN3_PIN, false);
    relayWrite(RELAY_IN4_PIN, false);
    return;
  }
  // Solid power/status
  relayWrite(RELAY_IN1_PIN, true);
  // Exactly one category
  relayWrite(RELAY_IN2_PIN, opClass == "COM");
  relayWrite(RELAY_IN3_PIN, opClass == "PVT");
  relayWrite(RELAY_IN4_PIN, opClass == "MIL");
}

#ifndef SCREEN_WIDTH
#define SCREEN_WIDTH 128
#endif
#ifndef SCREEN_HEIGHT
#define SCREEN_HEIGHT 64
#endif

// SPI OLED (SSD1322) via U8g2, full framebuffer, HW SPI (rotated 180°)
U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI u8g2(U8G2_R2, PIN_CS, PIN_DC, PIN_RST);
static WebServer g_server(80);

static const uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;  // 20s
static const uint32_t FETCH_INTERVAL_MS = 30000;        // 30s between API calls
static const uint32_t HTTP_CONNECT_TIMEOUT_MS = 15000;   // HTTP connect timeout
static const uint32_t HTTP_READ_TIMEOUT_MS = 30000;      // HTTP read timeout
// Preallocated HTTP body buffer to reduce heap churn
static const size_t BODY_BUF_CAP = 65536;  // 64KB cap
static char* g_bodyBuf = nullptr;
// Wi‑Fi reconnect state
static bool wifiConnecting = false;
static bool wifiInitialized = false;
static bool wifiEverBegun = false;

// MIL cache
struct MilCacheEntry { String hex; uint32_t ts; bool isMil; };
static const size_t MIL_CACHE_SIZE = 16;
static MilCacheEntry g_milCache[MIL_CACHE_SIZE];

static bool milCacheLookup(const String &hex, bool &isMilOut) {
  String key = hex; key.trim(); key.toUpperCase();
  uint32_t now = millis();
  const uint32_t TTL = 6UL * 60UL * 60UL * 1000UL; // 6 hours
  for (size_t i = 0; i < MIL_CACHE_SIZE; ++i) {
    if (g_milCache[i].hex.length() && g_milCache[i].hex.equalsIgnoreCase(key)) {
      if ((now - g_milCache[i].ts) < TTL) { isMilOut = g_milCache[i].isMil; return true; }
    }
  }
  return false;
}

static void milCacheStore(const String &hex, bool isMil) {
  String key = hex; key.trim(); key.toUpperCase();
  uint32_t now = millis();
  // Update existing
  for (size_t i = 0; i < MIL_CACHE_SIZE; ++i) {
    if (g_milCache[i].hex.length() && g_milCache[i].hex.equalsIgnoreCase(key)) {
      g_milCache[i].isMil = isMil; g_milCache[i].ts = now; return;
    }
  }
  // Insert into first empty or oldest
  size_t idx = MIL_CACHE_SIZE;
  uint32_t oldest = UINT32_MAX;
  for (size_t i = 0; i < MIL_CACHE_SIZE; ++i) {
    if (g_milCache[i].hex.length() == 0) { idx = i; break; }
    uint32_t age = now - g_milCache[i].ts;
    if (age > oldest) { oldest = age; idx = i; }
  }
  if (idx >= MIL_CACHE_SIZE) idx = 0;
  g_milCache[idx].hex = key; g_milCache[idx].isMil = isMil; g_milCache[idx].ts = now;
}

static bool fetchIsMilitaryByHex(const String &hex, bool &isMilOut) {
  isMilOut = false;
  if (WiFi.status() != WL_CONNECTED || hex.length() == 0) return false;
  String url = String(API_BASE);
  if (!url.startsWith("http")) url = String("https://") + url;
  url += "/v2/mil";
  Serial.print("[HTTP] GET "); Serial.println(url);

  HTTPClient http;
  http.setReuse(false);
  http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);
  http.setTimeout(HTTP_READ_TIMEOUT_MS);

  WiFiClientSecure client; client.setInsecure();
  if (!http.begin(client, url)) { Serial.println("[HTTP] begin() failed (mil)"); return false; }
  http.addHeader("Accept", "application/json");
  http.addHeader("Accept-Encoding", "identity");
  http.addHeader("Connection", "close");
  http.addHeader("User-Agent", "ESP32-FlightDisplay/1.0");

  int code = http.GET();
  Serial.print("[HTTP] Status (mil): "); Serial.println(code);
  if (code != HTTP_CODE_OK) { http.end(); return false; }

  String lowerHex = String(hex);
  lowerHex.toLowerCase();
  String needle = String("\"hex\":\"") + lowerHex + "\"";
  String carry;
  const size_t CHUNK = 1024;
  char buf[CHUNK + 1];
  unsigned long deadline = millis() + HTTP_READ_TIMEOUT_MS;
  Stream &s = http.getStream();
  while (millis() < deadline) {
    int n = s.readBytes(buf, CHUNK);
    if (n <= 0) { delay(10); yield(); if (!s.available()) break; else continue; }
    deadline = millis() + HTTP_READ_TIMEOUT_MS; // extend
    buf[n] = '\0';
    String chunk = carry + String(buf);
    chunk.toLowerCase();
    if (chunk.indexOf(needle) >= 0) { isMilOut = true; http.end(); return true; }
    // keep tail overlap
    if ((size_t)n >= needle.length()) carry = String(buf + n - needle.length());
    else carry = String(buf, n);
  }
  http.end();
  return true; // completed scan, not found => not mil
}

struct FlightInfo {
  String ident;     // flight/callsign or registration/hex fallback
  String typeCode;  // aircraft type (t)
  String category;  // raw category code
  long altitudeFt = -1;
  double lat = NAN;
  double lon = NAN;
  double distanceKm = NAN;
  String hex;       // transponder hex id
  bool hasCallsign = false;
  String opClass;   // MIL/COM/PVT
  bool valid = false;
  int seatOverride = -1; // if >0, override seat display
};

// Shared display state so network and test endpoints can update consistently
static bool g_haveDisplayed = false;
static FlightInfo g_lastShown;

static String classifyOp(const FlightInfo &fi) {
  // 1) Military always wins regardless of size
  if (fi.hex.length()) {
    bool isMil;
    if (milCacheLookup(fi.hex, isMil)) {
      if (isMil) return String("MIL");
    } else {
      bool ok = fetchIsMilitaryByHex(fi.hex, isMil);
      if (ok) milCacheStore(fi.hex, isMil);
      if (isMil) return String("MIL");
    }
  }

  // 2) Seat-based override: small aircraft (<= 15 seats) are PVT for this device.
  if (fi.typeCode.length()) {
    uint16_t maxSeats = 0;
    if (aircraftSeatMax(fi.typeCode, maxSeats)) {
      if (maxSeats > 0 && maxSeats <= 15) {
        return String("PVT");
      }
    }
  }

  // 3) Default: COM if it carries a callsign, else PVT
  return fi.hasCallsign ? String("COM") : String("PVT");
}

static bool sameFlightDisplay(const FlightInfo &a, const FlightInfo &b) {
  if (!a.valid && !b.valid) return true;
  if (a.valid != b.valid) return false;
  if (a.ident != b.ident) return false;
  if (a.typeCode != b.typeCode) return false;
  if (a.altitudeFt != b.altitudeFt) return false;
  if (a.opClass != b.opClass) return false;
  // Consider distances equal within 0.1 km to avoid flicker
  double da = isnan(a.distanceKm) ? 0 : a.distanceKm;
  double db = isnan(b.distanceKm) ? 0 : b.distanceKm;
  if (fabs(da - db) > 0.1) return false;
  return true;
}

static void drawCentered(const String &text, int16_t baselineY) {
  uint16_t w = u8g2.getUTF8Width(text.c_str());
  int16_t x = (SCREEN_WIDTH - (int)w) / 2;
  if (x < 0) x = 0;
  u8g2.drawUTF8(x, baselineY, text.c_str());
}

static void showSplash(const char *msgTop, const char *msgBottom = nullptr) {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);

  // Fonts
  const uint8_t* titleFont = u8g2_font_10x20_tf;
  const uint8_t* bodyFont  = u8g2_font_6x12_tf;

  // Measure line heights
  u8g2.setFont(titleFont);
  int16_t titleAscent = u8g2.getAscent();
  int16_t titleDescent = -u8g2.getDescent();
  int16_t titleH = titleAscent + titleDescent;

  u8g2.setFont(bodyFont);
  int16_t bodyAscent = u8g2.getAscent();
  int16_t bodyDescent = -u8g2.getDescent();
  int16_t bodyH = bodyAscent + bodyDescent;

  const int16_t gap = 6; // vertical spacing between lines
  int16_t totalH = titleH + gap + bodyH + ((msgBottom) ? (gap + bodyH) : 0);
  if (totalH < 0) totalH = 0;
  int16_t y0 = (SCREEN_HEIGHT - totalH) / 2;

  // Draw title centered
  u8g2.setFont(titleFont);
  int16_t base = y0 + titleAscent;
  drawCentered("Flight Display", base);

  // Draw first message
  u8g2.setFont(bodyFont);
  base = y0 + titleH + gap + bodyAscent;
  drawCentered(String(msgTop), base);

  // Optional second message
  if (msgBottom) {
    base = y0 + titleH + gap + bodyH + gap + bodyAscent;
    drawCentered(String(msgBottom), base);
  }

  u8g2.sendBuffer();
}

static void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  // Configure once in setup; do not change config while connecting
  if (!wifiInitialized) return;
  // Begin only once; rely on auto-reconnect afterwards
  if (wifiEverBegun) return;
  Serial.print("[WiFi] Connecting to "); Serial.println(WIFI_SSID);
  showSplash("Connecting Wi-Fi...", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  wifiConnecting = true;
  wifiEverBegun = true;
}

static double deg2rad(double deg) {
  return deg * PI / 180.0;
}
static double haversineKm(double lat1, double lon1, double lat2, double lon2) {
  const double R = 6371.0;  // km
  double dLat = deg2rad(lat2 - lat1);
  double dLon = deg2rad(lon2 - lon1);
  double a = sin(dLat / 2) * sin(dLat / 2) + cos(deg2rad(lat1)) * cos(deg2rad(lat2)) * sin(dLon / 2) * sin(dLon / 2);
  double c = 2 * atan2(sqrt(a), sqrt(1 - a));
  return R * c;
}

// Maximum acceptable age for a position (seconds). Align with tar1090 defaults (~15s),
// but allow up to 45s to be tolerant of intermittent updates on ESP32.
#ifndef POSITION_MAX_AGE_S
#define POSITION_MAX_AGE_S 45
#endif

static bool extractLatLon(JsonObject obj, double &outLat, double &outLon) {
  // Only accept positions that are not too stale
  if (obj.containsKey("seen_pos")) {
    double seenPos = obj["seen_pos"].as<double>();
    if (seenPos > POSITION_MAX_AGE_S) return false;
  }
  if (obj.containsKey("lat") && obj.containsKey("lon")) {
    outLat = obj["lat"].as<double>();
    outLon = obj["lon"].as<double>();
    if (!(outLat == 0.0 && outLon == 0.0)) return true; // ignore (0,0)
  }
  // Do not use lastPosition fallback to avoid selecting stale tracks
  return false;
}

static FlightInfo parseClosest(JsonVariant root) {
  FlightInfo res;
  if (!root.is<JsonObject>()) return res;
  JsonObject robj = root.as<JsonObject>();
  if (!robj.containsKey("ac") || !robj["ac"].is<JsonArray>()) return res;
  JsonArray ac = robj["ac"].as<JsonArray>();
  if (ac.size() == 0) return res;
  JsonObject obj = ac[0].as<JsonObject>();

  double lat, lon;
  if (!extractLatLon(obj, lat, lon)) return res;

  // Build ident preference chain: flight -> r -> hex
  String ident;
  bool hasCallsign = false;
  if (obj["flight"]) { ident = String(obj["flight"].as<const char *>()); hasCallsign = ident.length() > 0; }
  else if (obj["r"]) ident = String(obj["r"].as<const char *>());
  else if (obj["hex"]) ident = String(obj["hex"].as<const char *>());
  else ident = String("(unknown)");
  ident.trim();

  long alt = obj["alt_baro"].isNull() ? -1 : obj["alt_baro"].as<long>();
  String type = obj["t"].isNull() ? String("") : String(obj["t"].as<const char *>());
  if (!obj["t"] && obj["type"]) type = String(obj["type"].as<const char *>());
  String cat = obj["category"].isNull() ? String("") : String(obj["category"].as<const char *>());

  res.valid = true;
  res.ident = ident;
  res.typeCode = type;
  res.category = cat;
  res.altitudeFt = alt;
  res.lat = lat;
  res.lon = lon;
  res.distanceKm = haversineKm(HOME_LAT, HOME_LON, lat, lon); // 2D ground distance for display only
  res.hex = obj["hex"].isNull() ? String("") : String(obj["hex"].as<const char*>());
  res.hasCallsign = hasCallsign;
  return res;
}

static bool fetchNearestFlight(FlightInfo &out) {
  if (WiFi.status() != WL_CONNECTED) return false;

  auto buildUrl = [] (bool tls) {
    String base = String(API_BASE);
    if (tls) {
      if (base.startsWith("http://")) base.replace("http://", "https://");
      if (!base.startsWith("http")) base = String("https://") + base;
    } else {
      if (base.startsWith("https://")) base.replace("https://", "http://");
      if (!base.startsWith("http")) base = String("http://") + base;
    }
    base += "/v2/closest/";
    base += String(HOME_LAT, 6);
    base += "/";
    base += String(HOME_LON, 6);
    base += "/";
    base += String(SEARCH_RADIUS_KM);
    return base;
  };

  // Single HTTPS attempt; server enforces HTTPS and may close HTTP
  String url = buildUrl(true);
  Serial.print("[HTTP] GET "); Serial.println(url);
  Serial.print("[WiFi] RSSI: "); Serial.println(WiFi.RSSI());
  Serial.print("[MEM] Free heap: "); Serial.println(ESP.getFreeHeap());

  HTTPClient http;
  http.setReuse(false);
  http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);
  http.setTimeout(HTTP_READ_TIMEOUT_MS);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  WiFiClientSecure client; client.setInsecure();
  uint32_t clientTimeoutSec = (HTTP_READ_TIMEOUT_MS + 999) / 1000;
  client.setTimeout(clientTimeoutSec);
  if (!http.begin(client, url)) { Serial.println("[HTTP] begin() failed (TLS)"); return false; }
  http.addHeader("Accept", "application/json");
  http.addHeader("Accept-Encoding", "identity");
  http.addHeader("Connection", "close");
  http.addHeader("User-Agent", "ESP32-FlightDisplay/1.0");

  int code = http.GET();
  Serial.print("[HTTP] Status: "); Serial.println(code);
  if (code != HTTP_CODE_OK) {
    Serial.print("[HTTP] Error: "); Serial.println(http.errorToString(code));
    http.end();
    return false;
  }

  size_t contentLength = http.getSize();
  Serial.print("[HTTP] Content-Length: "); Serial.println(contentLength);

  // Build a filter to only keep what we need
  StaticJsonDocument<256> filter;
  JsonArray acArr = filter.createNestedArray("ac");
  JsonObject acObj = acArr.createNestedObject();
  acObj["flight"] = true;
  acObj["r"] = true;
  acObj["hex"] = true;
  acObj["t"] = true;
  acObj["type"] = true;
  acObj["alt_baro"] = true;
  acObj["lat"] = true;
  acObj["lon"] = true;
  // closest endpoint returns only one aircraft, but keep minimal fields
  acObj["seen_pos"] = true;

  DynamicJsonDocument doc(49152); // ~48KB; holds filtered subset

  if (contentLength > 0 && contentLength < 200000) {
    // Read exact bytes into a reusable buffer to avoid heap fragmentation
    if (!g_bodyBuf || contentLength + 1 > BODY_BUF_CAP) {
      Serial.println("[MEM] Body buffer not available or too small.");
      http.end();
      return false;
    }
    Stream &s = http.getStream();
    size_t total = 0;
    unsigned long deadline = millis() + HTTP_READ_TIMEOUT_MS;
    while (total < contentLength && millis() < deadline) {
      int avail = s.available();
      if (avail > 0) {
        size_t chunk = (size_t)avail;
        size_t remain = contentLength - total;
        if (chunk > remain) chunk = remain;
        int r = s.readBytes(g_bodyBuf + total, chunk);
        if (r > 0) {
          total += (size_t)r;
          deadline = millis() + HTTP_READ_TIMEOUT_MS; // extend deadline on progress
        } else {
          delay(10);
          yield();
        }
      } else {
        delay(10);
        yield();
      }
    }
    g_bodyBuf[total] = '\0';
    Serial.print("[HTTP] Read bytes: "); Serial.println(total);
    if (total != contentLength) {
      Serial.println("[HTTP] Warning: body shorter than Content-Length.");
    }
    http.end();
    DeserializationError err = deserializeJson(doc, g_bodyBuf, total, DeserializationOption::Filter(filter));
    if (err) {
      Serial.print("[JSON] Parse error: "); Serial.println(err.c_str());
      Serial.print("[HTTP] Body preview (512): "); Serial.println(String(g_bodyBuf).substring(0, 512));
      return false;
    }
  } else {
    // Unknown length; fall back to streamed parsing
    DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
    http.end();
    if (err) {
      Serial.print("[JSON] Parse error (streamed): "); Serial.println(err.c_str());
      return false;
    }
  }

  FlightInfo closest = parseClosest(doc.as<JsonVariant>());
  if (closest.valid) {
    Serial.print("[JSON] Closest: "); Serial.print(closest.ident);
    Serial.print("  dist "); Serial.print(closest.distanceKm, 2); Serial.println(" km");
    // Classify operation (MIL/COM/PVT)
    closest.opClass = classifyOp(closest);
    Serial.print("[CLASS] op: "); Serial.println(closest.opClass);
    out = closest;
    return true;
  }
  Serial.println("[JSON] No valid aircraft found in response.");
  return false;
}

// Apply a provided JSON payload (same schema as /v2/closest) to update display
static bool applyClosestJson(const char* json, size_t len, FlightInfo &out) {
  if (!json || len == 0) return false;
  // Build a filter to only keep what we need
  StaticJsonDocument<256> filter;
  JsonArray acArr = filter.createNestedArray("ac");
  JsonObject acObj = acArr.createNestedObject();
  acObj["flight"] = true;
  acObj["r"] = true;
  acObj["hex"] = true;
  acObj["t"] = true;
  acObj["type"] = true;
  acObj["alt_baro"] = true;
  acObj["lat"] = true;
  acObj["lon"] = true;
  acObj["seen_pos"] = true;

  DynamicJsonDocument doc(49152);
  DeserializationError err = deserializeJson(doc, json, len, DeserializationOption::Filter(filter));
  if (err) {
    Serial.print("[TEST] JSON parse error: "); Serial.println(err.c_str());
    return false;
  }
  FlightInfo closest = parseClosest(doc.as<JsonVariant>());
  if (!closest.valid) return false;
  closest.opClass = classifyOp(closest);
  out = closest;
  return true;
}

static void handleTestClosestPut() {
  // Expect application/json body matching /v2/closest response
  if (!g_server.hasArg("plain")) {
    g_server.send(400, "text/plain", "Missing body");
    return;
  }
  String body = g_server.arg("plain");
  Serial.print("[TEST] PUT /test/closest body bytes: "); Serial.println(body.length());
  FlightInfo fi;
  // Try simple schema first
  {
    StaticJsonDocument<1024> doc;
    DeserializationError err = deserializeJson(doc, body);
    if (!err && doc.is<JsonObject>() && !doc.containsKey("ac")) {
      JsonObject o = doc.as<JsonObject>();
      String code;
      if (o["t"]) code = String(o["t"].as<const char*>());
      else if (o["type"]) code = String(o["type"].as<const char*>());
      code.trim();
      if (code.length() > 0) {
        fi.valid = true;
        fi.typeCode = code;
        fi.ident = o["ident"].isNull() ? code : String(o["ident"].as<const char*>());
        fi.altitudeFt = o["alt"].isNull() ? -1 : (long)o["alt"].as<long>();
        fi.distanceKm = o["dist"].isNull() ? NAN : o["dist"].as<double>();
        if (!o["op"].isNull()) { fi.opClass = String(o["op"].as<const char*>()); fi.opClass.trim(); fi.opClass.toUpperCase(); }
        if (!o["seats"].isNull()) { int s = o["seats"].as<int>(); if (s > 0) fi.seatOverride = s; }
      }
    }
  }
  // If not valid, fall back to full /v2/closest schema
  if (!fi.valid) {
    if (!applyClosestJson(body.c_str(), body.length(), fi)) {
      g_server.send(400, "text/plain", "Invalid JSON");
      return;
    }
  }
  // Render and update relays immediately
  renderFlight(fi);
  g_lastShown = fi;
  g_haveDisplayed = true;
  updateRelaysForState(true, fi.opClass, false);
  g_server.send(200, "application/json", String("{\"status\":\"ok\",\"ident\":\"") + fi.ident + "\"}");
}

static void renderFlight(const FlightInfo &fi) {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);

  // 1) Top line: friendly aircraft name, as large as fits in one line
  String friendly = fi.typeCode.length() ? aircraftFriendlyName(fi.typeCode) : String("");
  if (!friendly.length() && fi.typeCode.length()) friendly = aircraftDisplayType(fi.typeCode);
  if (!friendly.length()) friendly = String("Unknown");

  // Bottom metrics font (~50% larger than before)
  const uint8_t* bottomFont = u8g2_font_9x18_tf; // was 6x12
  u8g2.setFont(bottomFont);
  int16_t bottomAscent = u8g2.getAscent();
  int16_t bottomDescent = -u8g2.getDescent();
  int16_t bottomH = bottomAscent + bottomDescent;
  const int16_t gapTopBottom = 2; // minimal gap above bottom line
  const int16_t topAvail = SCREEN_HEIGHT - bottomH - gapTopBottom;

  // Title fonts from largest to smaller; allow wrapping to up to 2 lines
  const uint8_t* titleFonts[] = {
    u8g2_font_logisoso32_tf,
    u8g2_font_logisoso24_tf,
    u8g2_font_logisoso20_tf,
    u8g2_font_10x20_tf,
    u8g2_font_9x15_tf,
    u8g2_font_6x12_tf
  };
  const size_t NFONTS = sizeof(titleFonts)/sizeof(titleFonts[0]);

  String line1 = friendly;
  String line2 = String("");
  const uint8_t* chosen = titleFonts[NFONTS - 1];
  auto tryWrapTwoLines = [&](const String &s, String &o1, String &o2) -> bool {
    // Attempt to split at spaces; choose split minimizing max line width and fitting within width
    o1 = s; o2 = String("");
    int bestIdx = -1; int bestWorst = INT_MAX;
    for (int i = 1; i < (int)s.length() - 1; ++i) {
      if (s[i] != ' ') continue;
      String a = s.substring(0, i);
      String b = s.substring(i + 1);
      uint16_t wa = u8g2.getUTF8Width(a.c_str());
      uint16_t wb = u8g2.getUTF8Width(b.c_str());
      if (wa <= SCREEN_WIDTH && wb <= SCREEN_WIDTH) {
        int worst = max((int)wa, (int)wb);
        if (worst < bestWorst) { bestWorst = worst; bestIdx = i; o1 = a; o2 = b; }
      }
    }
    return bestIdx >= 0;
  };

  for (size_t i = 0; i < NFONTS; ++i) {
    u8g2.setFont(titleFonts[i]);
    int16_t asc = u8g2.getAscent();
    int16_t desc = -u8g2.getDescent();
    int16_t lh = asc + desc;
    // One-line fit within width and height
    if (u8g2.getUTF8Width(friendly.c_str()) <= SCREEN_WIDTH && lh <= topAvail) {
      chosen = titleFonts[i];
      line1 = friendly; line2 = String("");
      break;
    }
    // Try two-line wrap for this font if two lines fit height
    if (2 * lh + 2 <= topAvail) {
      String a, b;
      if (tryWrapTwoLines(friendly, a, b)) {
        chosen = titleFonts[i];
        line1 = a; line2 = b;
        break;
      }
    }
  }

  // Draw title (one or two lines), vertically centered within topAvail
  u8g2.setFont(chosen);
  int16_t ascT = u8g2.getAscent();
  int16_t descT = -u8g2.getDescent();
  int16_t lhT = ascT + descT;
  int16_t totalTitleH = lhT + ((line2.length() > 0) ? (2 + lhT) : 0);
  if (totalTitleH > topAvail) totalTitleH = topAvail; // safety
  int16_t yStart = (topAvail - totalTitleH) / 2 + ascT;
  drawCentered(line1, yStart);
  if (line2.length() > 0) {
    drawCentered(line2, yStart + lhT + 2);
  }

  // 2) Bottom line: distance, seats, altitude (small font), evenly spaced across bottom
  u8g2.setFont(bottomFont);
  int16_t descent = u8g2.getDescent();
  int16_t yBottom = SCREEN_HEIGHT - 1 - (descent < 0 ? -descent : descent);

  String distStr = String("n/a");
  if (!isnan(fi.distanceKm)) distStr = String(fi.distanceKm, 1) + " km";

  uint16_t maxSeats = 0;
  String seatsStr = String("n/a");
  if (fi.seatOverride > 0) seatsStr = String(fi.seatOverride);
  else if (fi.typeCode.length() && aircraftSeatMax(fi.typeCode, maxSeats) && maxSeats > 0) seatsStr = String(maxSeats);

  String altStr = String("n/a");
  if (fi.altitudeFt >= 0) altStr = String(fi.altitudeFt) + " ft";

  const int cells = 3;
  const int cellW = SCREEN_WIDTH / cells;
  auto drawCenteredInCell = [&](int idx, const String &text){
    uint16_t bw = u8g2.getUTF8Width(text.c_str());
    int16_t cx = idx * cellW + (cellW - (int)bw) / 2;
    if (cx < 0) cx = 0;
    u8g2.drawUTF8(cx, yBottom, text.c_str());
  };

  drawCenteredInCell(0, distStr);
  drawCenteredInCell(1, seatsStr);
  drawCenteredInCell(2, altStr);

  u8g2.sendBuffer();
}

void setup() {
  // Preload inactive output level before switching to OUTPUT to avoid glitches
  const int INACTIVE = (RELAY_ACTIVE_HIGH ? LOW : HIGH);
  digitalWrite(RELAY_IN1_PIN, INACTIVE);
  digitalWrite(RELAY_IN2_PIN, INACTIVE);
  digitalWrite(RELAY_IN3_PIN, INACTIVE);
  digitalWrite(RELAY_IN4_PIN, INACTIVE);
  if (RELAY_IN1_PIN >= 0) pinMode(RELAY_IN1_PIN, OUTPUT);
  if (RELAY_IN2_PIN >= 0) pinMode(RELAY_IN2_PIN, OUTPUT);
  if (RELAY_IN3_PIN >= 0) pinMode(RELAY_IN3_PIN, OUTPUT);
  if (RELAY_IN4_PIN >= 0) pinMode(RELAY_IN4_PIN, OUTPUT);
  if (RELAY_IN1_PIN >= 0) digitalWrite(RELAY_IN1_PIN, INACTIVE);
  if (RELAY_IN2_PIN >= 0) digitalWrite(RELAY_IN2_PIN, INACTIVE);
  if (RELAY_IN3_PIN >= 0) digitalWrite(RELAY_IN3_PIN, INACTIVE);
  if (RELAY_IN4_PIN >= 0) digitalWrite(RELAY_IN4_PIN, INACTIVE);

  Serial.begin(115200);
  delay(20);
  Serial.println("\n[Boot] Flight Display starting...");
#if defined(ESP32)
  auto rr = esp_reset_reason();
  Serial.print("[Boot] Reset reason: ");
  switch (rr) {
    case ESP_RST_POWERON: Serial.println("POWERON"); break;
    case ESP_RST_EXT:     Serial.println("EXT"); break;
    case ESP_RST_SW:      Serial.println("SW"); break;
    case ESP_RST_PANIC:   Serial.println("PANIC"); break;
    case ESP_RST_INT_WDT: Serial.println("INT_WDT"); break;
    case ESP_RST_TASK_WDT:Serial.println("TASK_WDT"); break;
    case ESP_RST_WDT:     Serial.println("WDT"); break;
    case ESP_RST_DEEPSLEEP: Serial.println("DEEPSLEEP"); break;
    case ESP_RST_BROWNOUT: Serial.println("BROWNOUT"); break;
    case ESP_RST_SDIO:    Serial.println("SDIO"); break;
    default:              Serial.println((int)rr); break;
  }
#endif
  // Allocate reusable body buffer once
  g_bodyBuf = (char*)malloc(BODY_BUF_CAP);
  if (g_bodyBuf) {
    Serial.print("[MEM] Body buffer: "); Serial.print(BODY_BUF_CAP); Serial.println(" bytes");
  } else {
    Serial.println("[MEM] Body buffer allocation failed");
  }
  // Wi‑Fi event logging and dynamic power management
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info){
    switch(event) {
      case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        Serial.print("[WiFi] Disconnected. Reason: "); Serial.println(info.wifi_sta_disconnected.reason);
        wifiConnecting = false;
        // Drop TX power while attempting to reconnect to reduce current spikes
        WiFi.setTxPower(WIFI_BOOT_TXPOWER);
        WiFi.setSleep(true);
        break;
      case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        Serial.print("[WiFi] Got IP: "); Serial.println(IPAddress(info.got_ip.ip_info.ip.addr));
        wifiConnecting = false;
        // Raise TX power after association; disable modem sleep for responsiveness
        WiFi.setTxPower(WIFI_RUN_TXPOWER);
        WiFi.setSleep(false);
        // Wi‑Fi up; print reminder of server endpoint
        Serial.println("[HTTP] Server ready on port 80");
        break;
      default: break;
    }
  });
  // Configure Wi‑Fi once
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  // Start with low TX power and enable sleep for gentle connect
  WiFi.setTxPower(WIFI_BOOT_TXPOWER);
  WiFi.setSleep(true);
  wifiInitialized = true;
  // Display init (U8g2 handles SPI HW init for HW SPI constructor)
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.sendBuffer();

  showSplash("Booting...");
  // Allow power rails to settle before enabling Wi‑Fi/TLS
  delay(BOOT_POWER_SETTLE_MS);
  connectWiFi();

  // Start HTTP test server regardless of Wi‑Fi state; it will accept once IP is assigned
  g_server.on("/test/closest", HTTP_PUT, handleTestClosestPut);
  g_server.onNotFound([](){ g_server.send(404, "text/plain", "Not Found"); });
  g_server.begin();
  Serial.println("[HTTP] Test server listening on /test/closest (PUT)");
}

void loop() {
  static uint32_t lastFetch = 0;
  static uint32_t lastBlink = 0;
  static bool blinkPhase = false;

  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  if (millis() - lastFetch >= FETCH_INTERVAL_MS || lastFetch == 0) {
    lastFetch = millis();
    FlightInfo nearest;
    if (fetchNearestFlight(nearest)) {
      if (!g_haveDisplayed || !sameFlightDisplay(nearest, g_lastShown)) {
        renderFlight(nearest);
        g_lastShown = nearest;
        g_haveDisplayed = true;
        updateRelaysForState(true, g_lastShown.opClass, false);
      }
    } else {
      if (!g_haveDisplayed) {
        showSplash("No data", "Check Wi-Fi/API");
      }
    }
  }

  // Blink status relay until first display
  if (!g_haveDisplayed) {
    if (millis() - lastBlink >= 500) {
      lastBlink = millis();
      blinkPhase = !blinkPhase;
      updateRelaysForState(false, String(), blinkPhase);
    }
  }

  // Handle incoming HTTP requests for test endpoint
  g_server.handleClient();

  delay(100);
}
