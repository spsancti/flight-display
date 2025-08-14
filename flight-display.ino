// ESP32 ADS-B Flight Display (Proof of Concept)
// - Connects to Wi‑Fi
// - Calls adsb.lol /v2/point/{lat}/{lon}/{radius}
// - Parses nearest aircraft and renders summary on SSD1306 OLED

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#if defined(ESP32)
#include <esp_system.h>
#endif

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

#ifndef OLED_RESET_PIN
#define OLED_RESET_PIN -1
#endif

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET_PIN);

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
};

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
    uint16_t minSeats = 0, maxSeats = 0;
    if (aircraftSeatRange(fi.typeCode, minSeats, maxSeats)) {
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

static void drawCentered(const String &text, int16_t y, uint8_t size = 1) {
  display.setTextSize(size);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text.c_str(), 0, 0, &x1, &y1, &w, &h);
  int16_t x = (SCREEN_WIDTH - (int)w) / 2;
  if (x < 0) x = 0;
  display.setCursor(x, y);
  display.print(text);
}

static void showSplash(const char *msgTop, const char *msgBottom = nullptr) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  drawCentered("Flight Display", 0, 1);
  drawCentered(String(msgTop), 20, 1);
  if (msgBottom) drawCentered(String(msgBottom), 35, 1);
  display.display();
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

static void renderFlight(const FlightInfo &fi) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Closest Aircraft");

  // Friendly aircraft type name on second line
  display.setCursor(0, 12);
  String friendly = fi.typeCode.length() ? aircraftFriendlyName(fi.typeCode) : String("");
  if (!friendly.length() && fi.typeCode.length()) {
    // fallback to CODE Name
    friendly = aircraftDisplayType(fi.typeCode);
  }
  if (!friendly.length()) friendly = String("Unknown");
  if (friendly.length() > 21) friendly.remove(21);
  display.println(friendly);

  // Third line: upper seat limit if known
  display.setCursor(0, 22);
  uint16_t minSeats = 0, maxSeats = 0;
  if (fi.typeCode.length() && aircraftSeatRange(fi.typeCode, minSeats, maxSeats) && maxSeats > 0) {
    display.print("Seats: ");
    display.println(maxSeats);
  } else {
    display.println("Seats: n/a");
  }

  // Fourth line: operator class (MIL/COM/PVT)
  display.setCursor(0, 32);
  display.print("Op: ");
  if (fi.opClass.length()) display.println(fi.opClass);
  else display.println("n/a");

  display.setCursor(0, 42);
  display.print("Alt: ");
  if (fi.altitudeFt >= 0) {
    display.print(fi.altitudeFt);
    display.print(" ft");
  } else {
    display.print("n/a");
  }

  display.setCursor(0, 52);
  display.print("Dist: ");
  if (!isnan(fi.distanceKm)) {
    display.print(String(fi.distanceKm, 1));
    display.print(" km");
  } else {
    display.print("n/a");
  }

  display.display();
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
  // I2C + Display
  Wire.begin(OLED_SDA, OLED_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  display.display();

  showSplash("Booting...");
  // Allow power rails to settle before enabling Wi‑Fi/TLS
  delay(BOOT_POWER_SETTLE_MS);
  connectWiFi();
}

void loop() {
  static uint32_t lastFetch = 0;
  static bool haveDisplayed = false;
  static FlightInfo lastShown;
  static uint32_t lastBlink = 0;
  static bool blinkPhase = false;

  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  if (millis() - lastFetch >= FETCH_INTERVAL_MS || lastFetch == 0) {
    lastFetch = millis();
    FlightInfo nearest;
    if (fetchNearestFlight(nearest)) {
      if (!haveDisplayed || !sameFlightDisplay(nearest, lastShown)) {
        renderFlight(nearest);
        lastShown = nearest;
        haveDisplayed = true;
        updateRelaysForState(true, lastShown.opClass, false);
      }
    } else {
      if (!haveDisplayed) {
        showSplash("No data", "Check Wi-Fi/API");
      }
    }
  }

  // Blink status relay until first display
  if (!haveDisplayed) {
    if (millis() - lastBlink >= 500) {
      lastBlink = millis();
      blinkPhase = !blinkPhase;
      updateRelaysForState(false, String(), blinkPhase);
    }
  }

  delay(100);
}
