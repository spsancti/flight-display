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

#include "config.h"  // Create from config.example.h and do not commit secrets

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

struct FlightInfo {
  String ident;     // flight/callsign or registration/hex fallback
  String typeCode;  // aircraft type (t)
  String category;  // raw category code
  long altitudeFt = -1;
  double lat = NAN;
  double lon = NAN;
  double distanceKm = NAN;
  bool valid = false;
};

static bool sameFlightDisplay(const FlightInfo &a, const FlightInfo &b) {
  if (!a.valid && !b.valid) return true;
  if (a.valid != b.valid) return false;
  if (a.ident != b.ident) return false;
  if (a.typeCode != b.typeCode) return false;
  if (a.altitudeFt != b.altitudeFt) return false;
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
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WiFi] Connecting to ");
  Serial.println(WIFI_SSID);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_CONNECT_TIMEOUT_MS) {
    showSplash("Connecting Wi-Fi...", WIFI_SSID);
    delay(500);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[WiFi] Connected. IP: ");
    Serial.println(WiFi.localIP());
    showSplash("Wi-Fi connected", WiFi.localIP().toString().c_str());
    delay(500);
  } else {
    Serial.println("[WiFi] Connection failed. Check credentials.");
    showSplash("Wi-Fi failed", "Check credentials");
    delay(1500);
  }
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

static bool extractLatLon(JsonObject obj, double &outLat, double &outLon) {
  bool haveDirect = obj.containsKey("lat") && obj.containsKey("lon");
  if (haveDirect) {
    outLat = obj["lat"].as<double>();
    outLon = obj["lon"].as<double>();
    if (!(outLat == 0.0 && outLon == 0.0)) return true; // ignore (0,0)
  }
  if (obj.containsKey("lastPosition") && obj["lastPosition"].is<JsonObject>()) {
    JsonObject lp = obj["lastPosition"].as<JsonObject>();
    if (lp.containsKey("lat") && lp.containsKey("lon")) {
      outLat = lp["lat"].as<double>();
      outLon = lp["lon"].as<double>();
      if (!(outLat == 0.0 && outLon == 0.0)) return true;
    }
  }
  return false;
}

static FlightInfo parseNearest(JsonVariant root, size_t *outCount) {
  FlightInfo best;

  auto consider = [&](JsonObject obj) {
    double lat, lon;
    if (!extractLatLon(obj, lat, lon)) return;
    if (outCount) (*outCount)++;
    double d = haversineKm(HOME_LAT, HOME_LON, lat, lon);
    if (obj.containsKey("dst")) {
      // dst is typically nautical miles in many feeds; convert to km if present
      double dstNm = obj["dst"].as<double>();
      if (dstNm > 0) {
        double kmFromDst = dstNm * 1.852;
        // Prefer closer estimate; keep computed haversine as a sanity check
        if (!isnan(kmFromDst) && kmFromDst > 0) d = min(d, kmFromDst);
      }
    }
    // Build ident preference chain: flight -> r (registration) -> hex
    String ident;
    if (obj["flight"]) ident = String(obj["flight"].as<const char *>());
    else if (obj["r"]) ident = String(obj["r"].as<const char *>());
    else if (obj["hex"]) ident = String(obj["hex"].as<const char *>());
    else ident = String("(unknown)");
    ident.trim();

    long alt = obj["alt_baro"].isNull() ? -1 : obj["alt_baro"].as<long>();
    String type = obj["t"].isNull() ? String("") : String(obj["t"].as<const char *>());
    if (!obj["t"] && obj["type"]) type = String(obj["type"].as<const char *>());
    String cat = obj["category"].isNull() ? String("") : String(obj["category"].as<const char *>());

    if (!best.valid || d < best.distanceKm) {
      best.valid = true;
      best.ident = ident;
      best.typeCode = type;
      best.category = cat;
      best.altitudeFt = alt;
      best.lat = lat;
      best.lon = lon;
      best.distanceKm = d;
    }
  };

  if (root.is<JsonArray>()) {
    for (JsonObject obj : root.as<JsonArray>()) consider(obj);
  } else if (root.is<JsonObject>()) {
    JsonObject robj = root.as<JsonObject>();
    if (robj.containsKey("ac") && robj["ac"].is<JsonArray>()) {
      for (JsonObject obj : robj["ac"].as<JsonArray>()) consider(obj);
    } else if (robj.containsKey("aircraft") && robj["aircraft"].is<JsonArray>()) {
      for (JsonObject obj : robj["aircraft"].as<JsonArray>()) consider(obj);
    } else {
      // Fallback: treat object itself as single aircraft
      consider(robj);
    }
  }

  return best;
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
    base += "/v2/point/";
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
  acObj["dst"] = true;
  JsonObject lp = acObj.createNestedObject("lastPosition");
  lp["lat"] = true;
  lp["lon"] = true;

  DynamicJsonDocument doc(49152); // ~48KB; holds filtered subset

  if (contentLength > 0 && contentLength < 200000) {
    // Read exact bytes into a buffer with a resilient loop
    std::unique_ptr<char[]> buf(new (std::nothrow) char[contentLength + 1]);
    if (!buf) {
      Serial.println("[MEM] Allocation failed for body buffer.");
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
        int r = s.readBytes(buf.get() + total, chunk);
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
    buf[total] = '\0';
    Serial.print("[HTTP] Read bytes: "); Serial.println(total);
    if (total != contentLength) {
      Serial.println("[HTTP] Warning: body shorter than Content-Length.");
    }
    http.end();
    DeserializationError err = deserializeJson(doc, buf.get(), total, DeserializationOption::Filter(filter));
    if (err) {
      Serial.print("[JSON] Parse error: "); Serial.println(err.c_str());
      Serial.print("[HTTP] Body preview (512): "); Serial.println(String(buf.get()).substring(0, 512));
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

  size_t count = 0;
  FlightInfo best = parseNearest(doc.as<JsonVariant>(), &count);
  Serial.print("[JSON] Aircraft considered: "); Serial.println(count);
  if (best.valid) {
    Serial.print("[JSON] Nearest: "); Serial.print(best.ident);
    Serial.print("  dist "); Serial.print(best.distanceKm, 2); Serial.println(" km");
    out = best;
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
  display.println("Nearest Flight");

  display.setTextSize(2);
  display.setCursor(0, 12);
  display.println(fi.ident);

  display.setTextSize(1);
  display.setCursor(0, 32);
  display.print("Type: ");
  display.println(fi.typeCode.length() ? fi.typeCode : "n/a");

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
  Serial.begin(115200);
  delay(50);
  Serial.println("\n[Boot] Flight Display starting...");
  delay(200);
  // I2C + Display
  Wire.begin(OLED_SDA, OLED_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  display.display();

  showSplash("Booting...");
  delay(300);
  connectWiFi();
}

void loop() {
  static uint32_t lastFetch = 0;
  static bool haveDisplayed = false;
  static FlightInfo lastShown;

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
      }
    } else {
      if (!haveDisplayed) {
        showSplash("No data", "Check Wi-Fi/API");
      }
    }
  }

  delay(100);
}
