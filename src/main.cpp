// ESP32 ADS-B Flight Display
// - Connects to Wi-Fi
// - Calls adsb.lol /v2/closest/{lat}/{lon}/{radius}
// - Parses nearest aircraft and renders summary on a 466x466 round AMOLED

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#ifdef ESP32
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#endif
#include <ArduinoJson.h>
#include <Arduino_GFX_Library.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <WebServer.h>
#if defined(ESP32)
#include <esp_system.h>
#endif

#if __has_include("config.h")
#include "config.h"  // Create from example-config.h and do not commit secrets
#endif
#ifndef WIFI_SSID
#include "example-config.h"
#endif
#include "aircraft_types.h"
#include "log.h"

#include "display/drivers/AmoledDisplay/Amoled_DisplayPanel.h"
#include "display/drivers/AmoledDisplay/pin_config.h"

// FlightInfo is used across parsing, test overrides, and rendering.
// Define it early so it's a complete type wherever needed.
struct FlightInfo {
  String ident;     // flight/callsign or registration/hex fallback
  String typeCode;  // aircraft type (t)
  String category;  // raw category code
  long altitudeFt = -1;
  double lat = NAN;
  double lon = NAN;
  double distanceKm = NAN;
  String hex;  // transponder hex id
  bool hasCallsign = false;
  String opClass;  // MIL/COM/PVT
  bool valid = false;
  int seatOverride = -1;  // if >0, override seat display
};

static void renderSplash(const char *title, const char *subtitle = nullptr);
static void renderOtaProgress(uint8_t pct);
static void renderFlight(const FlightInfo &fi);
static void renderNoData(const char *detail);
static bool milCacheLookup(const String &hex, bool &outIsMil);
static void milCacheStore(const String &hex, bool isMil);
static bool fetchIsMilitaryByHex(const String &hex, bool &outIsMil);

// -----------------------------
// OTA configuration (overridable in config.h)
// -----------------------------
#ifndef FEATURE_OTA
#define FEATURE_OTA 1  // Set to 0 to disable Arduino OTA
#endif
#ifndef OTA_PORT
#define OTA_PORT 3232
#endif
#ifndef OTA_HOSTNAME
#define OTA_HOSTNAME "flight-display"
#endif
#ifndef OTA_PASSWORD
#define OTA_PASSWORD ""  // Empty = no auth (DEV ONLY). Set a strong password for production.
#endif

// Boot staging: allow power to settle before Wi-Fi/TLS ramps current
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

#ifndef FEATURE_MIL_LOOKUP
#define FEATURE_MIL_LOOKUP 1
#endif

// AMOLED panel selection (override in config.h)
#ifndef AMOLED_PANEL_WAVESHARE
#define AMOLED_PANEL_WAVESHARE 0
#endif
#ifndef AMOLED_PANEL_LILYGO
#define AMOLED_PANEL_LILYGO 1
#endif
#if AMOLED_PANEL_WAVESHARE && AMOLED_PANEL_LILYGO
#error "Select only one AMOLED panel configuration."
#endif

#ifndef AMOLED_BRIGHTNESS
#define AMOLED_BRIGHTNESS 12  // 1..16
#endif
#ifndef AMOLED_COLOR_ORDER
#define AMOLED_COLOR_ORDER ORDER_RGB
#endif

#if AMOLED_PANEL_WAVESHARE
constexpr AmoledHwConfig kHwConfig = WAVESHARE_S3_AMOLED_HW_CONFIG;
#else
constexpr AmoledHwConfig kHwConfig = LILYGO_T_DISPLAY_S3_DS_HW_CONFIG;
#endif

static Amoled_DisplayPanel g_panel(kHwConfig);
static Arduino_GFX *g_gfx = nullptr;
static int16_t g_screenW = 0;
static int16_t g_screenH = 0;
static int16_t g_centerX = 0;
static int16_t g_centerY = 0;
static int16_t g_safeRadius = 0;
static bool g_displayReady = false;

static const uint32_t FETCH_INTERVAL_MS = 30000;        // 30s between API calls
static const uint32_t HTTP_CONNECT_TIMEOUT_MS = 15000;  // HTTP connect timeout
static const uint32_t HTTP_READ_TIMEOUT_MS = 30000;     // HTTP read timeout
// Wi-Fi reconnect state
static bool wifiInitialized = false;
static bool wifiEverBegun = false;

#if FEATURE_OTA
static bool g_otaReady = false;        // true after ArduinoOTA.begin while Wi-Fi has IP
static volatile bool g_inOta = false;  // set during OTA to pause app work
#endif

// MIL cache
struct MilCacheEntry {
  String hex;
  uint32_t ts;
  bool isMil;
};
static const size_t MIL_CACHE_SIZE = 16;
static MilCacheEntry g_milCache[MIL_CACHE_SIZE];

// -----------------------------
// Test HTTP server (/test/closest)
// -----------------------------
#ifndef FEATURE_TEST_ENDPOINT
#define FEATURE_TEST_ENDPOINT 1  // Set 0 to remove test HTTP endpoint
#endif
#ifndef TEST_OVERRIDE_TTL_MS
#define TEST_OVERRIDE_TTL_MS (5UL * 60UL * 1000UL)  // 5 minutes
#endif

#if FEATURE_TEST_ENDPOINT
static WebServer g_http(80);
static bool g_httpStarted = false;
struct TestOverride {
  bool active = false;
  bool dirty = false;
  uint32_t expiresAt = 0;
  FlightInfo fi;
};
static TestOverride g_test;
#endif

// -----------------------------
// UI helpers
// -----------------------------
struct UiColors {
  uint16_t bg;
  uint16_t text;
  uint16_t muted;
  uint16_t accent;
  uint16_t com;
  uint16_t pvt;
  uint16_t mil;
  uint16_t warn;
  uint16_t ok;
};

static UiColors g_colors;

enum class WifiUiState {
  Offline,
  Connecting,
  Online,
};
static WifiUiState g_wifiUi = WifiUiState::Offline;

static uint32_t backoffMs(uint8_t attempt, uint32_t base = 350, uint32_t cap = 6000) {
  uint32_t exp = base << min<uint8_t>(attempt, 5);
  uint32_t j = (exp >> 3) * (esp_random() & 0x7) / 7;  // ~±12.5% jitter
  return min(cap, exp - (exp >> 4) + j);
}

static void waitMs(uint32_t durationMs) {
  uint32_t start = millis();
  while ((int32_t)(millis() - start) < (int32_t)durationMs) {
    yield();
  }
}

static void initUiColors() {
  g_colors.bg = BLACK;
  g_colors.text = WHITE;
  g_colors.muted = g_gfx->color565(150, 150, 150);
  g_colors.accent = g_gfx->color565(80, 200, 255);
  g_colors.com = g_gfx->color565(70, 160, 255);
  g_colors.pvt = g_gfx->color565(90, 220, 120);
  g_colors.mil = g_gfx->color565(255, 80, 80);
  g_colors.warn = g_gfx->color565(255, 190, 60);
  g_colors.ok = g_gfx->color565(80, 220, 160);
}

static void drawCenteredText(const String &text, int16_t centerX, int16_t centerY, const GFXfont *font, uint16_t color) {
  if (!g_gfx) return;
  g_gfx->setFont(font);
  g_gfx->setTextColor(color);
  g_gfx->setTextWrap(false);
  int16_t x1, y1;
  uint16_t w, h;
  g_gfx->getTextBounds(text.c_str(), 0, 0, &x1, &y1, &w, &h);
  int16_t x = centerX - (int16_t)(w / 2) - x1;
  int16_t y = centerY - (int16_t)(h / 2) - y1;
  g_gfx->setCursor(x, y);
  g_gfx->print(text);
}

static int16_t textHeightForFont(const GFXfont *font) {
  if (!g_gfx) return 0;
  g_gfx->setFont(font);
  int16_t x1, y1;
  uint16_t w, h;
  g_gfx->getTextBounds("Ag", 0, 0, &x1, &y1, &w, &h);
  return (int16_t)h;
}

static uint16_t textWidthForFont(const String &text, const GFXfont *font) {
  if (!g_gfx) return 0;
  g_gfx->setFont(font);
  int16_t x1, y1;
  uint16_t w, h;
  g_gfx->getTextBounds(text.c_str(), 0, 0, &x1, &y1, &w, &h);
  return w;
}

static bool splitTwoLines(const String &text, int16_t maxWidth, const GFXfont *font, String &line1, String &line2) {
  line1 = text;
  line2 = String("");
  int bestIdx = -1;
  int bestWorst = INT_MAX;
  for (int i = 1; i < (int)text.length() - 1; ++i) {
    if (text[i] != ' ') continue;
    String a = text.substring(0, i);
    String b = text.substring(i + 1);
    uint16_t wa = textWidthForFont(a, font);
    uint16_t wb = textWidthForFont(b, font);
    if (wa <= maxWidth && wb <= maxWidth) {
      int worst = max((int)wa, (int)wb);
      if (worst < bestWorst) {
        bestWorst = worst;
        bestIdx = i;
        line1 = a;
        line2 = b;
      }
    }
  }
  return bestIdx >= 0;
}

static void drawRing(uint16_t color, int16_t thickness = 3) {
  if (!g_gfx) return;
  for (int16_t t = 0; t < thickness; ++t) {
    g_gfx->drawCircle(g_centerX, g_centerY, g_safeRadius - t, color);
  }
}

static void drawBadge(const String &text, int16_t centerX, int16_t centerY, uint16_t bgColor, uint16_t textColor) {
  const GFXfont *font = &FreeSans9pt7b;
  uint16_t textW = textWidthForFont(text, font);
  int16_t textH = textHeightForFont(font);
  int16_t padX = 10;
  int16_t padY = 6;
  int16_t badgeW = textW + padX * 2;
  int16_t badgeH = textH + padY * 2;
  int16_t x = centerX - badgeW / 2;
  int16_t y = centerY - badgeH / 2;
  g_gfx->fillRoundRect(x, y, badgeW, badgeH, 8, bgColor);
  drawCenteredText(text, centerX, centerY, font, textColor);
}

static void drawStatusIndicators() {
  int16_t r = 6;
  int16_t x = g_centerX - g_safeRadius + 24;
  int16_t y = g_centerY - g_safeRadius + 24;
  uint16_t wifiColor = g_colors.warn;
  if (g_wifiUi == WifiUiState::Online) wifiColor = g_colors.ok;
  else if (g_wifiUi == WifiUiState::Connecting) wifiColor = g_colors.warn;
  else wifiColor = g_colors.mil;
  g_gfx->fillCircle(x, y, r, wifiColor);
  g_gfx->drawCircle(x, y, r, g_colors.text);

#if FEATURE_OTA
  uint16_t otaColor = g_inOta ? g_colors.accent : g_colors.muted;
  g_gfx->fillCircle(x + 24, y, r, otaColor);
  g_gfx->drawCircle(x + 24, y, r, g_colors.text);
#endif
}

static void drawRadialMetric(const String &value, const String &label, float angleDeg, uint16_t valueColor) {
  float radians = angleDeg * PI / 180.0f;
  int16_t valueRadius = g_safeRadius - 64;
  int16_t labelRadius = g_safeRadius - 36;
  int16_t valueX = g_centerX + (int16_t)(cosf(radians) * valueRadius);
  int16_t valueY = g_centerY + (int16_t)(sinf(radians) * valueRadius);
  int16_t labelX = g_centerX + (int16_t)(cosf(radians) * labelRadius);
  int16_t labelY = g_centerY + (int16_t)(sinf(radians) * labelRadius);
  drawCenteredText(value, valueX, valueY, &FreeSansBold18pt7b, valueColor);
  drawCenteredText(label, labelX, labelY, &FreeSans9pt7b, g_colors.muted);
}

static void drawCenteredTitle(const String &title, const String &subtitle) {
  const GFXfont *titleFonts[] = {
    &FreeSansBold24pt7b,
    &FreeSansBold18pt7b,
    &FreeSansBold12pt7b,
    &FreeSans12pt7b,
    &FreeSans9pt7b,
  };
  const size_t fontCount = sizeof(titleFonts) / sizeof(titleFonts[0]);
  int16_t maxWidth = g_safeRadius * 2 - 48;
  int16_t centerAreaRadius = g_safeRadius - 96;
  if (centerAreaRadius < 80) centerAreaRadius = g_safeRadius - 60;
  int16_t maxHeight = centerAreaRadius * 2;

  String line1 = title;
  String line2;
  const GFXfont *chosen = titleFonts[fontCount - 1];
  for (size_t i = 0; i < fontCount; ++i) {
    const GFXfont *font = titleFonts[i];
    uint16_t w = textWidthForFont(title, font);
    int16_t h = textHeightForFont(font);
    if (w <= maxWidth && h <= maxHeight) {
      chosen = font;
      line1 = title;
      line2 = String("");
      break;
    }
    String a, b;
    if (splitTwoLines(title, maxWidth, font, a, b)) {
      int16_t totalH = (2 * h) + 8;
      if (totalH <= maxHeight) {
        chosen = font;
        line1 = a;
        line2 = b;
        break;
      }
    }
  }

  int16_t titleH = textHeightForFont(chosen);
  int16_t totalTitleH = titleH + ((line2.length() > 0) ? (titleH + 8) : 0);
  int16_t baseY = g_centerY - (totalTitleH / 2);
  drawCenteredText(line1, g_centerX, baseY + titleH / 2, chosen, g_colors.text);
  if (line2.length() > 0) {
    drawCenteredText(line2, g_centerX, baseY + titleH + 8 + titleH / 2, chosen, g_colors.text);
  }

  if (subtitle.length()) {
    int16_t subtitleY = baseY + totalTitleH + 26;
    drawCenteredText(subtitle, g_centerX, subtitleY, &FreeSans9pt7b, g_colors.muted);
  }
}

static void drawBackground() {
  g_gfx->fillScreen(g_colors.bg);
  drawRing(g_colors.accent, 3);
  drawStatusIndicators();
}

static void renderSplash(const char *title, const char *subtitle) {
  if (!g_displayReady) return;
  drawBackground();
  drawCenteredTitle(String(title), subtitle ? String(subtitle) : String(""));
}

static void renderNoData(const char *detail) {
  if (!g_displayReady) return;
  drawBackground();
  drawCenteredTitle(String("No Data"), detail ? String(detail) : String(""));
}

static void renderOtaProgress(uint8_t pct) {
  if (!g_displayReady) return;
  char buf[24];
  snprintf(buf, sizeof(buf), "OTA %u%%", pct);
  drawBackground();
  drawCenteredTitle(String("Updating"), String(buf));
}

// (moved earlier)

// Shared display state so network and test endpoints can update consistently
static bool g_haveDisplayed = false;
static FlightInfo g_lastShown;

static String classifyOp(const FlightInfo &fi) {
  // 1) Military always wins regardless of size
  if (FEATURE_MIL_LOOKUP && fi.hex.length()) {
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

static void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  // Configure once in setup; do not change config while connecting
  if (!wifiInitialized) return;
  // Begin only once; rely on auto-reconnect afterwards
  if (wifiEverBegun) return;
  Serial.print("[WiFi] Connecting to ");
  Serial.println(WIFI_SSID);
  if (g_displayReady) {
    renderSplash("Connecting Wi-Fi...", WIFI_SSID);
  }
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  g_wifiUi = WifiUiState::Connecting;
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
    if (!(outLat == 0.0 && outLon == 0.0)) return true;  // ignore (0,0)
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
  if (obj["flight"]) {
    ident = String(obj["flight"].as<const char *>());
    hasCallsign = ident.length() > 0;
  } else if (obj["r"]) ident = String(obj["r"].as<const char *>());
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
  res.distanceKm = haversineKm(HOME_LAT, HOME_LON, lat, lon);  // 2D ground distance for display only
  res.hex = obj["hex"].isNull() ? String("") : String(obj["hex"].as<const char *>());
  res.hasCallsign = hasCallsign;
  return res;
}

static bool fetchNearestFlight(FlightInfo &out) {
  if (WiFi.status() != WL_CONNECTED) return false;

  auto buildUrl = [](bool tls) {
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
  LOG_INFO("HTTP GET %s", url.c_str());
  LOG_DEBUG("WiFi RSSI: %d dBm", WiFi.RSSI());
  LOG_DEBUG("Free heap: %u", (unsigned)ESP.getFreeHeap());

  HTTPClient http;
  http.setReuse(false);
  http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);
  http.setTimeout(HTTP_READ_TIMEOUT_MS);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  WiFiClientSecure client;
  client.setInsecure();
  uint32_t clientTimeoutSec = (HTTP_READ_TIMEOUT_MS + 999) / 1000;
  client.setTimeout(clientTimeoutSec);
  if (!http.begin(client, url)) {
    Serial.println("[HTTP] begin() failed (TLS)");
    return false;
  }
  http.addHeader("Accept", "application/json");
  http.addHeader("Accept-Encoding", "identity");
  http.addHeader("Connection", "close");
  http.addHeader("User-Agent", "ESP32-FlightDisplay/2.0");

  int code = http.GET();
  LOG_INFO("HTTP status: %d", code);
  if (code != HTTP_CODE_OK) {
    LOG_WARN("HTTP error: %s", http.errorToString(code).c_str());
    http.end();
    return false;
  }

  size_t contentLength = http.getSize();
  LOG_DEBUG("HTTP Content-Length: %u", (unsigned)contentLength);

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

  DynamicJsonDocument doc(8192);  // ~8KB; filtered and single-aircraft
  // Prefer streamed parsing to minimize RAM and fragmentation
  DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    LOG_WARN("JSON parse error (streamed): %s", err.c_str());
    return false;
  }

  FlightInfo closest = parseClosest(doc.as<JsonVariant>());
  if (closest.valid) {
    LOG_INFO("Closest %s  dist %.2f km", closest.ident.c_str(), closest.distanceKm);
    // Classify operation (MIL/COM/PVT)
    closest.opClass = classifyOp(closest);
    LOG_INFO("Classified op: %s", closest.opClass.c_str());
    out = closest;
    return true;
  }
  LOG_INFO("No valid aircraft found in response");
  return false;
}

// -----------------------------
// MIL cache helpers
// -----------------------------
static bool milCacheLookup(const String &hex, bool &outIsMil) {
  uint32_t now = millis();
  for (size_t i = 0; i < MIL_CACHE_SIZE; ++i) {
    if (g_milCache[i].hex == hex) {
      // 6h cache
      if (now - g_milCache[i].ts < 6UL * 60UL * 60UL * 1000UL) {
        outIsMil = g_milCache[i].isMil;
        return true;
      }
      return false;
    }
  }
  return false;
}

static void milCacheStore(const String &hex, bool isMil) {
  size_t slot = 0;
  uint32_t oldest = UINT32_MAX;
  for (size_t i = 0; i < MIL_CACHE_SIZE; ++i) {
    if (g_milCache[i].hex.length() == 0) {
      slot = i;
      break;
    }
    if (g_milCache[i].ts < oldest) {
      oldest = g_milCache[i].ts;
      slot = i;
    }
  }
  g_milCache[slot].hex = hex;
  g_milCache[slot].ts = millis();
  g_milCache[slot].isMil = isMil;
}

static bool fetchIsMilitaryByHex(const String &hex, bool &outIsMil) {
  if (WiFi.status() != WL_CONNECTED) return false;
  String url = String(API_BASE);
  if (url.startsWith("http://")) url.replace("http://", "https://");
  if (!url.startsWith("http")) url = String("https://") + url;
  url += "/v2/mil/";
  url += hex;

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10);
  HTTPClient http;
  http.setConnectTimeout(8000);
  http.setTimeout(10000);
  if (!http.begin(client, url)) return false;
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }
  DynamicJsonDocument doc(512);
  DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();
  if (err) return false;
  outIsMil = doc["is_military"] | false;
  return true;
}

#if FEATURE_TEST_ENDPOINT
static void httpStartOnce() {
  if (g_httpStarted) return;
  g_http.on("/test/closest", HTTP_PUT, []() {
    if (!g_http.hasArg("plain")) {
      g_http.send(400, "text/plain", "Missing body");
      return;
    }
    StaticJsonDocument<2048> doc;
    DeserializationError err = deserializeJson(doc, g_http.arg("plain"));
    if (err) {
      g_http.send(400, "text/plain", "Invalid JSON");
      return;
    }

    FlightInfo fi;
    JsonVariant root = doc.as<JsonVariant>();
    if (root.is<JsonObject>() && root.as<JsonObject>().containsKey("ac")) {
      fi = parseClosest(root);
    } else {
      fi.valid = true;
      fi.typeCode = doc["t"] | "";
      fi.ident = doc["ident"] | "TEST";
      fi.altitudeFt = doc["alt"] | -1;
      fi.distanceKm = doc["dist"] | NAN;
      fi.hasCallsign = fi.ident.length() > 0;
    }
    if (fi.valid) {
      fi.opClass = classifyOp(fi);
      g_test.fi = fi;
      g_test.active = true;
      g_test.dirty = true;
      g_test.expiresAt = millis() + TEST_OVERRIDE_TTL_MS;
      g_http.send(200, "text/plain", "OK");
    } else {
      g_http.send(400, "text/plain", "Invalid payload");
    }
  });
  g_http.begin();
  g_httpStarted = true;
  Serial.println(F("[HTTP] Test endpoint ready: PUT /test/closest"));
}
#endif

static bool initDisplay() {
  for (uint8_t attempt = 0; attempt < 4; ++attempt) {
    if (g_panel.begin(AMOLED_COLOR_ORDER)) {
      g_panel.setBrightness(AMOLED_BRIGHTNESS);
      g_gfx = g_panel.gfx();
      g_screenW = g_panel.width();
      g_screenH = g_panel.height();
      g_centerX = g_screenW / 2;
      g_centerY = g_screenH / 2;
      g_safeRadius = min(g_centerX, g_centerY) - 18;
      initUiColors();
      g_gfx->fillScreen(g_colors.bg);
      g_displayReady = true;
      return true;
    }
    waitMs(backoffMs(attempt));
  }
  return false;
}

static void renderFlight(const FlightInfo &fi) {
  if (!g_displayReady) return;

  // 1) friendly aircraft name
  String friendly = fi.typeCode.length() ? aircraftFriendlyName(fi.typeCode) : String("");
  bool isPseudo = false;
  String codeUC = fi.typeCode;
  codeUC.trim();
  codeUC.toUpperCase();
  if (!friendly.length() && codeUC.length()) {
    if (codeUC.startsWith("TISB")) {
      friendly = "TIS-B Target";
      isPseudo = true;
    } else if (codeUC.startsWith("ADSB")) {
      friendly = "ADS-B Target";
      isPseudo = true;
    } else if (codeUC.startsWith("MLAT")) {
      friendly = "MLAT Target";
      isPseudo = true;
    } else if (codeUC.startsWith("MODE")) {
      friendly = "Mode-S Target";
      isPseudo = true;
    }
  }
  if (!friendly.length()) friendly = String("Unknown Aircraft");

  drawBackground();

  // Op class badge near top
  uint16_t badgeColor = g_colors.pvt;
  if (fi.opClass == "COM") badgeColor = g_colors.com;
  else if (fi.opClass == "MIL") badgeColor = g_colors.mil;
  drawBadge(fi.opClass, g_centerX, g_centerY - g_safeRadius + 42, badgeColor, g_colors.text);

  String callsign = fi.ident.length() ? fi.ident : String("—");
  drawCenteredTitle(friendly, callsign);

  // Metrics around the ring
  String distStr = String("—");
  if (!isnan(fi.distanceKm)) distStr = String(fi.distanceKm, 1) + " km";

  uint16_t maxSeats = 0;
  String seatsStr = String("—");
  if (!isPseudo) {
    if (fi.seatOverride > 0) seatsStr = String(fi.seatOverride);
    else if (fi.typeCode.length() && aircraftSeatMax(fi.typeCode, maxSeats) && maxSeats > 0) seatsStr = String(maxSeats);
  }

  String altStr = String("—");
  if (fi.altitudeFt >= 0) altStr = String(fi.altitudeFt) + " ft";

  drawRadialMetric(distStr, "DIST", 225.0f, g_colors.accent);
  drawRadialMetric(seatsStr, "SEATS", 90.0f, g_colors.ok);
  drawRadialMetric(altStr, "ALT", 315.0f, g_colors.warn);
}

#if FEATURE_OTA
static void otaBeginOnce() {
  if (g_otaReady) return;
  ArduinoOTA.setPort(OTA_PORT);
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  if (strlen(OTA_PASSWORD) > 0) ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() {
    g_inOta = true;
    LOG_WARN("OTA start");
  });
  ArduinoOTA.onEnd([]() {
    LOG_WARN("OTA end");
    g_inOta = false;
  });
  ArduinoOTA.onProgress([](unsigned progress, unsigned total) {
    static uint32_t lastDraw = 0;
    uint32_t now = millis();
    if (now - lastDraw < 150) return;  // rate-limit drawing
    lastDraw = now;
    uint8_t pct = (total ? (progress * 100U / total) : 0);
    renderOtaProgress(pct);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    LOG_ERROR("OTA error: %d", (int)error);
    g_inOta = false;  // allow app to resume
  });
  ArduinoOTA.begin();
  g_otaReady = true;
  LOG_INFO("OTA ready: %s.local:%d", OTA_HOSTNAME, OTA_PORT);
}
#endif

void setup() {
  Serial.begin(115200);
  waitMs(20);
  Serial.println(F("\n[Boot] Flight Display starting..."));
#if defined(ESP32)
  auto rr = esp_reset_reason();
  Serial.print(F("[Boot] Reset reason: "));
  switch (rr) {
    case ESP_RST_POWERON: Serial.println(F("POWERON")); break;
    case ESP_RST_EXT: Serial.println(F("EXT")); break;
    case ESP_RST_SW: Serial.println(F("SW")); break;
    case ESP_RST_PANIC: Serial.println(F("PANIC")); break;
    case ESP_RST_INT_WDT: Serial.println(F("INT_WDT")); break;
    case ESP_RST_TASK_WDT: Serial.println(F("TASK_WDT")); break;
    case ESP_RST_WDT: Serial.println(F("WDT")); break;
    case ESP_RST_DEEPSLEEP: Serial.println(F("DEEPSLEEP")); break;
    case ESP_RST_BROWNOUT: Serial.println(F("BROWNOUT")); break;
    case ESP_RST_SDIO: Serial.println(F("SDIO")); break;
    default: Serial.println((int)rr); break;
  }
#endif

  if (!initDisplay()) {
    Serial.println(F("[Display] AMOLED init failed"));
  }

  if (g_displayReady) {
    renderSplash("Booting...");
  }

  // Wi-Fi event logging and dynamic power management
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
      case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        LOG_WARN("WiFi disconnected. Reason: %d", info.wifi_sta_disconnected.reason);
        g_wifiUi = WifiUiState::Offline;
        // Drop TX power while attempting to reconnect to reduce current spikes
        WiFi.setTxPower(WIFI_BOOT_TXPOWER);
        WiFi.setSleep(true);
#if FEATURE_OTA
        g_otaReady = false;
        g_inOta = false;
#endif
        break;
      case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        Serial.print(F("[WiFi] Got IP: "));
        Serial.println(IPAddress(info.got_ip.ip_info.ip.addr));
        g_wifiUi = WifiUiState::Online;
        // Raise TX power after association; disable modem sleep for responsiveness
        WiFi.setTxPower(WIFI_RUN_TXPOWER);
        WiFi.setSleep(false);
        // Wi-Fi up; print reminder of server endpoint
        Serial.println(F("[HTTP] Server ready on port 80"));
#if FEATURE_OTA
        otaBeginOnce();
#endif
#if FEATURE_TEST_ENDPOINT
        httpStartOnce();
#endif
        break;
      default: break;
    }
  });
  // Configure Wi-Fi once
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  // Start with low TX power and enable sleep for gentle connect
  WiFi.setTxPower(WIFI_BOOT_TXPOWER);
  WiFi.setSleep(true);
  wifiInitialized = true;

  // Allow power rails to settle before enabling Wi-Fi/TLS
  waitMs(BOOT_POWER_SETTLE_MS);
  connectWiFi();

#if FEATURE_TEST_ENDPOINT
  // Start HTTP listener early; it will become reachable after Wi-Fi is up
  httpStartOnce();
#endif
}

void loop() {
  static uint32_t lastFetch = 0;

  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

#if FEATURE_TEST_ENDPOINT
  if (g_httpStarted) {
    g_http.handleClient();
  }
#endif

#if FEATURE_OTA
  if (g_otaReady) {
    ArduinoOTA.handle();
  }
  if (g_inOta) {
    // Skip all other work during OTA to ensure smooth flashing
    yield();
    return;
  }
#endif

#if FEATURE_TEST_ENDPOINT
  if (g_test.active && (int32_t)(millis() - g_test.expiresAt) < 0) {
    if (g_test.dirty || !g_haveDisplayed || !sameFlightDisplay(g_test.fi, g_lastShown)) {
      renderFlight(g_test.fi);
      g_lastShown = g_test.fi;
      g_haveDisplayed = true;
      g_test.dirty = false;
    }
    // Skip network fetch while override active
    yield();
    return;
  }
#endif

  if (millis() - lastFetch >= FETCH_INTERVAL_MS || lastFetch == 0) {
    lastFetch = millis();
    FlightInfo nearest;
    if (fetchNearestFlight(nearest)) {
      if (!g_haveDisplayed || !sameFlightDisplay(nearest, g_lastShown)) {
        renderFlight(nearest);
        g_lastShown = nearest;
        g_haveDisplayed = true;
      }
    } else {
      if (!g_haveDisplayed) {
        renderNoData("Check Wi-Fi/API");
      }
    }
  }

  // Cooperative yield without blocking
  yield();
}
