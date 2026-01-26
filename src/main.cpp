// ESP32 ADS-B Flight Display
// - Connects to Wi-Fi
// - Calls adsb.lol /v2/point/{lat}/{lon}/{radius}
// - Parses nearest aircraft and renders summary on a 466x466 round AMOLED

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Arduino_GFX_Library.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <WebServer.h>
#include <Wire.h>
#include <lvgl.h>
#include "display/drivers/common/LV_Helper.h"
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#if defined(ESP32)
#include <esp_system.h>
#include <esp_adc_cal.h>
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
  String route;    // route string (e.g., TLV-RMO)
  bool valid = false;
  int seatOverride = -1;  // if >0, override seat display
};

static void renderSplash(const char *title, const char *subtitle = nullptr);
static void renderFlight(const FlightInfo &fi);
static void renderNoData(const char *detail);
static bool milCacheLookup(const String &hex, bool &outIsMil);
static void milCacheStore(const String &hex, bool isMil);
static bool fetchIsMilitaryByHex(const String &hex, bool &outIsMil);
static bool fetchRouteByCallsign(const String &callsign, double lat, double lon, String &outRoute);
static void fetchTask(void *arg);

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
#ifndef FEATURE_ROUTE_LOOKUP
#define FEATURE_ROUTE_LOOKUP 1
#endif
#ifndef ROUTE_CACHE_TTL_MS
#define ROUTE_CACHE_TTL_MS (6UL * 60UL * 60UL * 1000UL)
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
#ifndef TOUCH_BRIGHTNESS_MAX
#define TOUCH_BRIGHTNESS_MAX 12
#endif
#ifndef TOUCH_BRIGHTNESS_MIN
#define TOUCH_BRIGHTNESS_MIN 2
#endif
#ifndef TOUCH_BRIGHTNESS_MS
#define TOUCH_BRIGHTNESS_MS 30000
#endif
#ifndef CLOSE_RADIUS_KM
#define CLOSE_RADIUS_KM 4
#endif
#ifndef TOUCH_IDLE_SLEEP_MS
#define TOUCH_IDLE_SLEEP_MS (15UL * 60UL * 1000UL)
#endif
#ifndef BATTERY_UI_UPDATE_MS
#define BATTERY_UI_UPDATE_MS 5000
#endif
#ifndef SLEEP_BUTTON_PIN
#define SLEEP_BUTTON_PIN 0
#endif
#ifndef SLEEP_HOLD_MS
#define SLEEP_HOLD_MS 1500
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
static uint32_t g_lastTouchMs = 0;
static uint8_t g_lastBrightness = 0;
static uint32_t g_sleepHoldStartMs = 0;
static uint32_t g_touchBoostUntilMs = 0;
static portMUX_TYPE g_flightMux = portMUX_INITIALIZER_UNLOCKED;
static FlightInfo g_pendingFlight;
static bool g_pendingValid = false;
static uint32_t g_pendingSeq = 0;

static const uint32_t FETCH_INTERVAL_MS = 3000;        // 30s between API calls
static const uint32_t HTTP_CONNECT_TIMEOUT_MS = 15000;  // HTTP connect timeout
static const uint32_t HTTP_READ_TIMEOUT_MS = 30000;     // HTTP read timeout
// Wi-Fi reconnect state
static bool wifiInitialized = false;
static bool wifiEverBegun = false;

// MIL cache
struct MilCacheEntry {
  String hex;
  uint32_t ts;
  bool isMil;
};
static const size_t MIL_CACHE_SIZE = 16;
static MilCacheEntry g_milCache[MIL_CACHE_SIZE];

struct RouteCacheEntry {
  String callsign;
  String route;
  uint32_t ts = 0;
};
static RouteCacheEntry g_routeCache;

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
  uint16_t bezel;
  uint16_t bezelBorder;
  uint16_t screen;
  uint16_t screenBorder;
  uint16_t text;
  uint16_t muted;
  uint16_t label;
  uint16_t green;
  uint16_t greenDim;
  uint16_t com;
  uint16_t pvt;
  uint16_t mil;
  uint16_t warn;
  uint16_t ok;
};

static UiColors g_colors;
static int16_t g_panelR = 0;
static int16_t g_windowX = 0;
static int16_t g_windowY = 0;
static int16_t g_windowW = 0;
static int16_t g_windowH = 0;
static int16_t g_windowR = 0;
static int16_t g_labelY = 0;

struct UiLvColors {
  lv_color_t bg;
  lv_color_t bezel;
  lv_color_t bezelBorder;
  lv_color_t screen;
  lv_color_t screenBorder;
  lv_color_t text;
  lv_color_t muted;
  lv_color_t label;
  lv_color_t green;
  lv_color_t greenDim;
  lv_color_t pvt;
  lv_color_t com;
  lv_color_t mil;
  lv_color_t ledOff;
};

struct UiLvWidgets {
  lv_obj_t *bezel = nullptr;
  lv_obj_t *window = nullptr;
  lv_obj_t *title = nullptr;
  lv_obj_t *subtitle = nullptr;
  lv_obj_t *route = nullptr;
  lv_obj_t *timeLbl = nullptr;
  lv_obj_t *battLbl = nullptr;
  lv_obj_t *metricVal[3] = {nullptr, nullptr, nullptr};
  lv_obj_t *metricLbl[3] = {nullptr, nullptr, nullptr};
  lv_obj_t *ledBtn[3] = {nullptr, nullptr, nullptr};
  lv_obj_t *ledLbl[3] = {nullptr, nullptr, nullptr};
};

static UiLvColors g_lvColors;
static UiLvWidgets g_lv;
static bool g_lvReady = false;

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

static uint8_t clampBrightness(int value) {
  if (value < 1) return 1;
  if (value > 16) return 16;
  return static_cast<uint8_t>(value);
}

static void initUiColors() {
  g_colors.bg = g_gfx->color565(6, 7, 8);
  g_colors.bezel = g_gfx->color565(18, 20, 22);
  g_colors.bezelBorder = g_gfx->color565(36, 38, 40);
  g_colors.screen = g_gfx->color565(9, 12, 10);
  g_colors.screenBorder = g_gfx->color565(22, 28, 22);
  g_colors.text = g_gfx->color565(230, 230, 230);
  g_colors.muted = g_gfx->color565(150, 155, 160);
  g_colors.label = g_gfx->color565(130, 130, 130);
  g_colors.green = g_gfx->color565(100, 255, 120);
  g_colors.greenDim = g_gfx->color565(60, 170, 80);
  g_colors.com = g_gfx->color565(250, 245, 235);
  g_colors.pvt = g_gfx->color565(230, 230, 230);
  g_colors.mil = g_gfx->color565(210, 30, 30);
  g_colors.warn = g_gfx->color565(255, 190, 60);
  g_colors.ok = g_gfx->color565(80, 220, 160);
}

static void computeLayout() {
  int16_t innerRadius = g_safeRadius - 12;
  g_panelR = innerRadius;

  g_windowW = (int16_t)(g_screenW * 0.90f) - 20;
  g_windowH = (int16_t)(g_screenH * 0.42f);
  if (g_windowW < 200) g_windowW = 200;
  if (g_windowH < 140) g_windowH = 140;
  int16_t maxW = g_safeRadius * 2 - 8;
  int16_t maxH = g_safeRadius * 2 - 120;
  if (g_windowW > maxW) g_windowW = maxW;
  if (g_windowH > maxH) g_windowH = maxH;
  g_windowX = g_centerX - g_windowW / 2;
  g_windowY = g_centerY - g_windowH / 2 - 10;
  g_windowR = 16;
  g_labelY = g_windowY + g_windowH + 18;
}

static void uiInit() {
  g_lvColors.bg = lv_color_hex(0x0A0B0C);
  g_lvColors.bezel = lv_color_hex(0x000000);
  g_lvColors.bezelBorder = lv_color_hex(0x000000);
  g_lvColors.screen = lv_color_hex(0x0A100B);
  g_lvColors.screenBorder = lv_color_hex(0x000000);
  g_lvColors.text = lv_color_hex(0xE6E6E6);
  g_lvColors.muted = lv_color_hex(0x9AA0A6);
  g_lvColors.label = lv_color_hex(0x7C7C7C);
  g_lvColors.green = lv_color_hex(0x64FF78);
  g_lvColors.greenDim = lv_color_hex(0x3CAA50);
  g_lvColors.pvt = lv_color_hex(0xE6E6E6);
  g_lvColors.com = lv_color_hex(0xFAF5EB);
  g_lvColors.mil = lv_color_hex(0xD21E1E);
  g_lvColors.ledOff = lv_color_hex(0x2F3336);

  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, g_lvColors.bg, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

  int16_t d = min(g_screenW, g_screenH) - 8;
  g_lv.bezel = lv_obj_create(scr);
  lv_obj_set_size(g_lv.bezel, d, d);
  lv_obj_set_style_radius(g_lv.bezel, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_lv.bezel, g_lvColors.bezel, LV_PART_MAIN);
  lv_obj_set_style_border_color(g_lv.bezel, g_lvColors.bezelBorder, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_lv.bezel, 2, LV_PART_MAIN);
  lv_obj_clear_flag(g_lv.bezel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(g_lv.bezel, g_centerX - d / 2, g_centerY - d / 2);

  g_lv.window = lv_obj_create(scr);
  lv_obj_set_size(g_lv.window, g_windowW, g_windowH);
  lv_obj_set_style_radius(g_lv.window, 14, LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_lv.window, g_lvColors.screen, LV_PART_MAIN);
  lv_obj_set_style_border_color(g_lv.window, g_lvColors.screenBorder, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_lv.window, 2, LV_PART_MAIN);
  lv_obj_clear_flag(g_lv.window, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(g_lv.window, g_windowX, g_windowY);

  g_lv.timeLbl = lv_label_create(scr);
  lv_label_set_text(g_lv.timeLbl, "");
  lv_obj_set_style_text_color(g_lv.timeLbl, g_lvColors.muted, LV_PART_MAIN);
  lv_obj_set_style_text_font(g_lv.timeLbl, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_align(g_lv.timeLbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_width(g_lv.timeLbl, 70);

  g_lv.battLbl = lv_label_create(scr);
  lv_label_set_text(g_lv.battLbl, "");
  lv_obj_set_style_text_color(g_lv.battLbl, g_lvColors.muted, LV_PART_MAIN);
  lv_obj_set_style_text_font(g_lv.battLbl, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_align(g_lv.battLbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_width(g_lv.battLbl, 70);

  g_lv.title = lv_label_create(g_lv.window);
  lv_label_set_long_mode(g_lv.title, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(g_lv.title, g_windowW - 16);
  lv_obj_set_style_text_color(g_lv.title, g_lvColors.green, LV_PART_MAIN);
  lv_obj_set_style_text_font(g_lv.title, &lv_font_montserrat_34, LV_PART_MAIN);
  lv_obj_set_style_text_align(g_lv.title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_align(g_lv.title, LV_ALIGN_CENTER, 0, -2);

  g_lv.subtitle = lv_label_create(g_lv.window);
  lv_label_set_long_mode(g_lv.subtitle, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(g_lv.subtitle, g_windowW - 16);
  lv_obj_set_style_text_color(g_lv.subtitle, g_lvColors.greenDim, LV_PART_MAIN);
  lv_obj_set_style_text_font(g_lv.subtitle, &lv_font_montserrat_24, LV_PART_MAIN);
  lv_obj_set_style_text_align(g_lv.subtitle, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_align(g_lv.subtitle, LV_ALIGN_TOP_MID, 0, 8);

  g_lv.route = lv_label_create(g_lv.window);
  lv_label_set_long_mode(g_lv.route, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(g_lv.route, g_windowW - 16);
  lv_obj_set_style_text_color(g_lv.route, g_lvColors.greenDim, LV_PART_MAIN);
  lv_obj_set_style_text_font(g_lv.route, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_set_style_text_align(g_lv.route, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_align(g_lv.route, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_label_set_text(g_lv.route, "TLV-RMO");

  static const char *kMetricLabels[3] = {"DIST", "SOULS", "ALT"};
  for (int i = 0; i < 3; ++i) {
    g_lv.metricLbl[i] = lv_label_create(scr);
    lv_label_set_text(g_lv.metricLbl[i], kMetricLabels[i]);
    lv_obj_set_style_text_color(g_lv.metricLbl[i], g_lvColors.label, LV_PART_MAIN);
    lv_obj_set_style_text_font(g_lv.metricLbl[i], &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(g_lv.metricLbl[i], 2, LV_PART_MAIN);
  }

  const float anglesDeg[3] = {238.0f, 270.0f, 302.0f};
  int16_t r = g_safeRadius - 8;
  for (int i = 0; i < 3; ++i) {
    float radians = anglesDeg[i] * PI / 180.0f;
    int16_t x = g_centerX + (int16_t)(cosf(radians) * r);
    int16_t y = g_centerY + (int16_t)(sinf(radians) * r);
    if (y < g_labelY) y = g_labelY;
    lv_obj_set_pos(g_lv.metricLbl[i], x - 28, y - 8);
  }

  for (int i = 0; i < 3; ++i) {
    g_lv.metricVal[i] = lv_label_create(scr);
    lv_obj_set_style_text_color(g_lv.metricVal[i], g_lvColors.green, LV_PART_MAIN);
    lv_obj_set_style_text_font(g_lv.metricVal[i], &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_align(g_lv.metricVal[i], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align_to(g_lv.metricVal[i], g_lv.metricLbl[i], LV_ALIGN_OUT_BOTTOM_MID, 0, 6);
  }

  static const char *kLedLabels[3] = {"PVT", "COM", "MIL"};
  int16_t btnW = 42;
  int16_t btnH = 22;
  {
    int16_t gap = 6;
    int16_t totalW = (btnW * 3) + (gap * 2);
    int16_t startX = g_centerX - totalW / 2;
    int16_t midY = lv_obj_get_y(g_lv.metricVal[1]) + lv_obj_get_height(g_lv.metricVal[1]) + 6 + (btnH / 2);
    for (int i = 0; i < 3; ++i) {
      g_lv.ledBtn[i] = lv_obj_create(scr);
      lv_obj_set_size(g_lv.ledBtn[i], btnW, btnH);
      lv_obj_set_style_radius(g_lv.ledBtn[i], 6, LV_PART_MAIN);
      lv_obj_set_style_bg_color(g_lv.ledBtn[i], g_lvColors.ledOff, LV_PART_MAIN);
      lv_obj_set_style_border_color(g_lv.ledBtn[i], g_lvColors.label, LV_PART_MAIN);
      lv_obj_set_style_border_width(g_lv.ledBtn[i], 1, LV_PART_MAIN);
      lv_obj_set_style_pad_all(g_lv.ledBtn[i], 0, LV_PART_MAIN);
      lv_obj_clear_flag(g_lv.ledBtn[i], LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_pos(g_lv.ledBtn[i], startX + i * (btnW + gap), midY);

      g_lv.ledLbl[i] = lv_label_create(g_lv.ledBtn[i]);
      lv_label_set_text(g_lv.ledLbl[i], kLedLabels[i]);
      lv_obj_set_style_text_color(g_lv.ledLbl[i], lv_color_hex(0x000000), LV_PART_MAIN);
      lv_obj_set_style_text_font(g_lv.ledLbl[i], &lv_font_montserrat_14, LV_PART_MAIN);
      lv_obj_center(g_lv.ledLbl[i]);
    }
  }

  {
    int16_t topY = g_centerY - g_safeRadius + 18;
    lv_obj_set_pos(g_lv.timeLbl, g_centerX - 100, topY);
    lv_obj_set_pos(g_lv.battLbl, g_centerX + 30, topY);
  }

  g_lvReady = true;
}

static void uiSetOpClass(const char *op) {
  if (!g_lvReady) return;
  const char *labels[3] = {"PVT", "COM", "MIL"};
  lv_color_t colors[3] = {g_lvColors.pvt, g_lvColors.com, g_lvColors.mil};
  for (int i = 0; i < 3; ++i) {
    bool isActive = op && strcmp(op, labels[i]) == 0;
    lv_color_t fill = isActive ? colors[i] : g_lvColors.ledOff;
    lv_color_t border = isActive ? colors[i] : g_lvColors.label;
    lv_color_t text = isActive ? lv_color_hex(0x000000) : g_lvColors.muted;
    lv_obj_set_style_bg_color(g_lv.ledBtn[i], fill, LV_PART_MAIN);
    lv_obj_set_style_border_color(g_lv.ledBtn[i], border, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_lv.ledLbl[i], text, LV_PART_MAIN);
  }
}

static void uiSetTitle(const String &title, const String &subtitle) {
  if (!g_lvReady) return;
  lv_label_set_text(g_lv.title, title.c_str());
  lv_label_set_text(g_lv.subtitle, subtitle.c_str());
  if (g_lv.route) {
    lv_label_set_text(g_lv.route, "TLV-RMO");
  }
}

static void uiSetMetrics(const String &dist, const String &seats, const String &alt) {
  if (!g_lvReady) return;
  lv_label_set_text(g_lv.metricVal[0], dist.c_str());
  lv_label_set_text(g_lv.metricVal[1], seats.c_str());
  lv_label_set_text(g_lv.metricVal[2], alt.c_str());
}

static void uiSetRoute(const String &route) {
  if (!g_lvReady || !g_lv.route) return;
  lv_label_set_text(g_lv.route, route.c_str());
}

static void uiSetBatteryMv(uint16_t mv, bool charging) {
  if (!g_lvReady || !g_lv.battLbl) return;
  if (mv == 0) {
    lv_label_set_text(g_lv.battLbl, "--.-V");
    lv_obj_set_style_text_color(g_lv.battLbl, g_lvColors.muted, LV_PART_MAIN);
    return;
  }
  char buf[12];
  uint16_t whole = mv / 1000;
  uint16_t frac = (mv % 1000) / 10;
  snprintf(buf, sizeof(buf), "%u.%02uV", whole, frac);
  lv_label_set_text(g_lv.battLbl, buf);
  lv_obj_set_style_text_color(g_lv.battLbl, charging ? g_lvColors.mil : g_lvColors.green, LV_PART_MAIN);
}

static void updateBatteryUi() {
  if (!g_displayReady || !g_lvReady) return;
  uint16_t mv = g_panel.getBattVoltage();
  bool charging = g_panel.isCharging();
  uiSetBatteryMv(mv, charging);
}

static void renderSplash(const char *title, const char *subtitle) {
  if (!g_displayReady || !g_lvReady) return;
  uiSetOpClass(nullptr);
  uiSetTitle(String(title), subtitle ? String(subtitle) : String(""));
  uiSetRoute(String("-"));
  uiSetMetrics(String("-"), String("-"), String("-"));
}

static void renderNoData(const char *detail) {
  if (!g_displayReady || !g_lvReady) return;
  uiSetOpClass(nullptr);
  uiSetTitle(String("No Data"), detail ? String(detail) : String(""));
  uiSetRoute(String("-"));
  uiSetMetrics(String("-"), String("-"), String("-"));
}

// (moved earlier)

// Shared display state so network and test endpoints can update consistently
static bool g_haveDisplayed = false;
static FlightInfo g_lastShown;

static String classifyOp(const FlightInfo &fi) {
  // 1) Military always wins regardless of size
  if (FEATURE_MIL_LOOKUP && fi.hex.length()) {
    bool isMil = false;
    if (milCacheLookup(fi.hex, isMil)) {
      if (isMil) return String("MIL");
    } else {
      bool ok = fetchIsMilitaryByHex(fi.hex, isMil);
      if (ok) milCacheStore(fi.hex, isMil);
      if (ok && isMil) return String("MIL");
    }
  }

  // 2) Seat-based override: small aircraft (<= 20 seats) are PVT for this device.
  if (fi.typeCode.length()) {
    uint16_t maxSeats = 0;
    if (aircraftSeatMax(fi.typeCode, maxSeats)) {
      if (maxSeats > 0 && maxSeats <= 20) {
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
  if (a.route != b.route) return false;
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
  LOG_INFO("WiFi connecting to %s", WIFI_SSID);
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

static bool parseAircraft(JsonObject obj, FlightInfo &res) {
  double lat, lon;
  if (!extractLatLon(obj, lat, lon)) return false;

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

  long alt = -1;
  if (!obj["alt_baro"].isNull()) alt = obj["alt_baro"].as<long>();
  else if (!obj["alt_geom"].isNull()) alt = obj["alt_geom"].as<long>();
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
  res.route = String("");
  return true;
}

static FlightInfo parseClosest(JsonVariant root) {
  FlightInfo res;
  if (!root.is<JsonObject>()) return res;
  JsonObject robj = root.as<JsonObject>();
  if (!robj.containsKey("ac") || !robj["ac"].is<JsonArray>()) return res;
  JsonArray ac = robj["ac"].as<JsonArray>();
  if (ac.size() == 0) return res;
  JsonObject obj = ac[0].as<JsonObject>();
  if (!parseAircraft(obj, res)) return FlightInfo{};
  return res;
}

static uint16_t radiusNmFromKm(double km) {
  if (km <= 0) return 0;
  double nm = km * 0.539957;  // km -> nautical miles
  uint16_t v = (uint16_t)(nm + 0.5);
  if (v == 0) v = 1;
  if (v > 250) v = 250;
  return v;
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
    base += "/v2/point/";
    base += String(HOME_LAT, 6);
    base += "/";
    base += String(HOME_LON, 6);
    base += "/";
    base += String(radiusNmFromKm(SEARCH_RADIUS_KM));
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
    LOG_ERROR("HTTP begin failed (TLS)");
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
  acObj["alt_geom"] = true;
  acObj["lat"] = true;
  acObj["lon"] = true;
  acObj["category"] = true;
  acObj["seen_pos"] = true;

  DynamicJsonDocument doc(12288);  // filtered; may include multiple aircraft
  // Prefer streamed parsing to minimize RAM and fragmentation
  DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    LOG_WARN("JSON parse error (streamed): %s", err.c_str());
    return false;
  }

  JsonVariant root = doc.as<JsonVariant>();
  if (!root.is<JsonObject>() || !root.as<JsonObject>()["ac"].is<JsonArray>()) {
    LOG_INFO("No valid aircraft list in response");
    return false;
  }
  JsonArray ac = root.as<JsonObject>()["ac"].as<JsonArray>();

  FlightInfo bestAir;
  FlightInfo bestGround;
  bool hasAir = false;
  bool hasGround = false;
  for (JsonVariant v : ac) {
    if (!v.is<JsonObject>()) continue;
    FlightInfo fi;
    if (!parseAircraft(v.as<JsonObject>(), fi)) continue;
    bool inFlight = fi.altitudeFt > 0;
    if (inFlight) {
      if (!hasAir || fi.distanceKm < bestAir.distanceKm) {
        bestAir = fi;
        hasAir = true;
      }
    } else {
      if (!hasGround || fi.distanceKm < bestGround.distanceKm) {
        bestGround = fi;
        hasGround = true;
      }
    }
  }

  if (!hasAir && !hasGround) {
    LOG_INFO("No valid aircraft found in response");
    return false;
  }

  FlightInfo closest = hasAir ? bestAir : bestGround;
  if (hasAir) {
    LOG_INFO("Closest airborne %s  dist %.2f km", closest.ident.c_str(), closest.distanceKm);
  } else {
    LOG_INFO("Closest grounded %s  dist %.2f km", closest.ident.c_str(), closest.distanceKm);
  }

  // Classify operation (MIL/COM/PVT)
  closest.opClass = classifyOp(closest);
  LOG_INFO("Classified op: %s", closest.opClass.c_str());
  if (FEATURE_ROUTE_LOOKUP && closest.hasCallsign) {
    bool cacheOk = false;
    if (g_routeCache.callsign == closest.ident &&
        (millis() - g_routeCache.ts) < ROUTE_CACHE_TTL_MS &&
        g_routeCache.route.length()) {
      closest.route = g_routeCache.route;
      cacheOk = true;
      LOG_INFO("Route cache hit for %s", closest.ident.c_str());
    }
    if (!cacheOk) {
      String route;
      if (fetchRouteByCallsign(closest.ident, closest.lat, closest.lon, route)) {
        closest.route = route;
        g_routeCache.callsign = closest.ident;
        g_routeCache.route = route;
        g_routeCache.ts = millis();
      } else {
        LOG_WARN("Route lookup failed for %s", closest.ident.c_str());
      }
    }
  } else if (FEATURE_ROUTE_LOOKUP && !closest.hasCallsign) {
    LOG_INFO("Route lookup skipped: no callsign for %s", closest.ident.c_str());
  }
  out = closest;
  return true;
}

static void fetchTask(void *arg) {
  (void)arg;
  uint32_t lastFetch = 0;
  for (;;) {
    uint32_t now = millis();
    if ((int32_t)(now - lastFetch) >= (int32_t)FETCH_INTERVAL_MS || lastFetch == 0) {
      lastFetch = now;
#if FEATURE_TEST_ENDPOINT
      if (g_test.active && (int32_t)(millis() - g_test.expiresAt) < 0) {
        vTaskDelay(pdMS_TO_TICKS(200));
        continue;
      }
#endif
      FlightInfo fi;
      bool ok = fetchNearestFlight(fi);
      portENTER_CRITICAL(&g_flightMux);
      g_pendingValid = ok;
      if (ok) {
        g_pendingFlight = fi;
      }
      g_pendingSeq++;
      portEXIT_CRITICAL(&g_flightMux);
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
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
  url += "/v2/mil";

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
  Stream &stream = http.getStream();
  String hexLower = hex;
  hexLower.toLowerCase();
  String hexUpper = hex;
  hexUpper.toUpperCase();
  String needleLower = String("\"hex\":\"") + hexLower + "\"";
  String needleUpper = String("\"hex\":\"") + hexUpper + "\"";
  const char *countNeedle = "\"hex\":\"";
  const size_t countNeedleLen = strlen(countNeedle);
  uint32_t hexCount = 0;
  String tail;
  char buf[160];
  bool found = false;
  while (http.connected() || stream.available()) {
    int n = stream.readBytes(buf, sizeof(buf) - 1);
    if (n <= 0) break;
    buf[n] = '\0';
    String chunk = tail + String(buf);
    int scanStart = 0;
    if ((int)tail.length() >= (int)(countNeedleLen - 1)) {
      scanStart = (int)tail.length() - (int)(countNeedleLen - 1);
    }
    const char *chunkC = chunk.c_str();
    int maxStart = (int)chunk.length() - (int)countNeedleLen;
    for (int i = scanStart; i <= maxStart; ++i) {
      if (memcmp(chunkC + i, countNeedle, countNeedleLen) == 0) {
        ++hexCount;
        i += (int)countNeedleLen - 1;
      }
    }
    if (chunk.indexOf(needleLower) >= 0 || chunk.indexOf(needleUpper) >= 0) {
      found = true;
      break;
    }
    int keep = max(0, (int)max(needleLower.length(), needleUpper.length()) - 1);
    if (chunk.length() > keep) tail = chunk.substring(chunk.length() - keep);
    else tail = chunk;
    yield();
  }
  LOG_INFO("Mil list entries: %lu", (unsigned long)hexCount);
  http.end();
  outIsMil = found;
  return true;
}

static bool fetchRouteByCallsign(const String &callsign, double lat, double lon, String &outRoute) {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (!callsign.length()) return false;

  String url = String(API_BASE);
  if (url.startsWith("http://")) url.replace("http://", "https://");
  if (!url.startsWith("http")) url = String("https://") + url;
  url += "/api/0/routeset";

  DynamicJsonDocument req(256);
  JsonArray planes = req.createNestedArray("planes");
  JsonObject p0 = planes.createNestedObject();
  p0["callsign"] = callsign;
  if (!isnan(lat)) p0["lat"] = lat;
  if (!isnan(lon)) p0["lng"] = lon;

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10);
  HTTPClient http;
  http.setConnectTimeout(8000);
  http.setTimeout(10000);
  if (!http.begin(client, url)) return false;
  http.addHeader("Content-Type", "application/json");
  String body;
  serializeJson(req, body);
  LOG_INFO("Route lookup POST %s callsign=%s", url.c_str(), callsign.c_str());
  int code = http.POST(body);
  LOG_INFO("Route lookup status: %d", code);
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  String resp = http.getString();
  http.end();
  // Raw response logging removed once parsing confirmed.
  DynamicJsonDocument doc(2048);
  DeserializationError err = deserializeJson(doc, resp);
  if (err) return false;
  LOG_DEBUG("Route lookup JSON ok");

  String route;
  if (doc.is<JsonArray>()) {
    JsonArray arr = doc.as<JsonArray>();
    if (arr.size() > 0) {
      JsonVariant v = arr[0];
      if (v.is<const char *>()) route = String(v.as<const char *>());
      else if (v.is<JsonObject>()) {
        JsonObject o = v.as<JsonObject>();
        if (o["_airport_codes_iata"]) route = String(o["_airport_codes_iata"].as<const char *>());
        else if (o["route"]) route = String(o["route"].as<const char *>());
        else if (o["routes"]) route = String(o["routes"].as<const char *>());
      }
    }
  } else if (doc.is<JsonObject>()) {
    JsonObject o = doc.as<JsonObject>();
    if (o["_airport_codes_iata"]) route = String(o["_airport_codes_iata"].as<const char *>());
    else if (o["route"]) route = String(o["route"].as<const char *>());
    else if (o["routes"]) route = String(o["routes"].as<const char *>());
    else if (o["result"]) route = String(o["result"].as<const char *>());
  } else if (doc.is<const char *>()) {
    route = String(doc.as<const char *>());
  }

  route.trim();
  if (!route.length()) return false;
  LOG_INFO("Route lookup result: %s", route.c_str());
  outRoute = route;
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
      fi.route = String("");
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
  LOG_INFO("HTTP test endpoint ready: PUT /test/closest");
}
#endif

static bool initDisplay() {
  for (uint8_t attempt = 0; attempt < 4; ++attempt) {
    if (g_panel.begin(AMOLED_COLOR_ORDER)) {
      g_panel.setBrightness(clampBrightness(TOUCH_BRIGHTNESS_MIN));
      g_lastBrightness = clampBrightness(TOUCH_BRIGHTNESS_MIN);
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

static bool initDisplayFromPanelReady() {
  g_gfx = g_panel.gfx();
  if (!g_gfx) return false;
  g_screenW = g_panel.width();
  g_screenH = g_panel.height();
  g_centerX = g_screenW / 2;
  g_centerY = g_screenH / 2;
  g_safeRadius = min(g_centerX, g_centerY) - 18;
  initUiColors();
  g_gfx->fillScreen(g_colors.bg);
  g_displayReady = true;
  g_lastBrightness = clampBrightness(g_panel.getBrightness());
  return true;
}

static void renderFlight(const FlightInfo &fi) {
  if (!g_displayReady || !g_lvReady) return;

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

  uiSetOpClass(fi.opClass.c_str());

  String callsign = fi.ident.length() ? fi.ident : String("-");
  uiSetTitle(friendly, callsign);
  uiSetRoute(fi.route.length() ? fi.route : String("-"));

  // Metrics around the ring
  String distStr = String("-");
  if (!isnan(fi.distanceKm)) distStr = String(fi.distanceKm, 1) + " km";

  uint16_t maxSeats = 0;
  String seatsStr = String("-");
  if (!isPseudo) {
    if (fi.seatOverride > 0) seatsStr = String(fi.seatOverride);
    else if (fi.typeCode.length() && aircraftSeatMax(fi.typeCode, maxSeats) && maxSeats > 0) seatsStr = String(maxSeats);
  }

  String altStr = String("-");
  if (fi.altitudeFt <= 0) {
    altStr = String("ground");
  } else {
    int meters = (int)(fi.altitudeFt * 0.3048 + 0.5);
    altStr = String(meters) + " m";
  }

  uiSetMetrics(distStr, seatsStr, altStr);
}

void setup() {
  Serial.begin(115200);
  waitMs(20);
  LOG_INFO("Boot: Flight Display starting...");
#if defined(ESP32)
  auto rr = esp_reset_reason();
  bool wokeFromSleep = (rr == ESP_RST_DEEPSLEEP);
  const char *rrStr = "UNKNOWN";
  switch (rr) {
    case ESP_RST_POWERON: rrStr = "POWERON"; break;
    case ESP_RST_EXT: rrStr = "EXT"; break;
    case ESP_RST_SW: rrStr = "SW"; break;
    case ESP_RST_PANIC: rrStr = "PANIC"; break;
    case ESP_RST_INT_WDT: rrStr = "INT_WDT"; break;
    case ESP_RST_TASK_WDT: rrStr = "TASK_WDT"; break;
    case ESP_RST_WDT: rrStr = "WDT"; break;
    case ESP_RST_DEEPSLEEP: rrStr = "DEEPSLEEP"; break;
    case ESP_RST_BROWNOUT: rrStr = "BROWNOUT"; break;
    case ESP_RST_SDIO: rrStr = "SDIO"; break;
    default: rrStr = "UNKNOWN"; break;
  }
  LOG_INFO("Boot: Reset reason %s (%d)", rrStr, (int)rr);
#endif

  bool displayOk = false;
#if defined(ESP32)
  if (wokeFromSleep) {
    displayOk = g_panel.wakeup();
    if (displayOk) {
      displayOk = initDisplayFromPanelReady();
      if (displayOk) {
        g_panel.setBrightness(clampBrightness(TOUCH_BRIGHTNESS_MIN));
        g_lastBrightness = clampBrightness(TOUCH_BRIGHTNESS_MIN);
        LOG_INFO("Display wakeup complete");
      }
    }
  }
#endif
  if (!displayOk) {
    displayOk = initDisplay();
  }
  if (!displayOk) {
    while (true) {
      LOG_ERROR("Display init failed");
      delay(1000);
    }
  }

  if (g_displayReady) {
    beginLvglHelper(g_panel, false);
    computeLayout();
    uiInit();
    renderSplash("Booting...");
  }

  pinMode(SLEEP_BUTTON_PIN, INPUT_PULLUP);
  g_lastTouchMs = millis();

  xTaskCreatePinnedToCore(fetchTask, "fetchTask", 8192, nullptr, 1, nullptr, 0);

  // Wi-Fi event logging and dynamic power management
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
      case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        LOG_WARN("WiFi disconnected. Reason: %d", info.wifi_sta_disconnected.reason);
        g_wifiUi = WifiUiState::Offline;
        // Drop TX power while attempting to reconnect to reduce current spikes
        WiFi.setTxPower(WIFI_BOOT_TXPOWER);
        WiFi.setSleep(true);
        break;
      case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        {
          IPAddress ip(info.got_ip.ip_info.ip.addr);
          String ipStr = ip.toString();
          LOG_INFO("WiFi got IP: %s", ipStr.c_str());
        }
        g_wifiUi = WifiUiState::Online;
        // Raise TX power after association; disable modem sleep for responsiveness
        WiFi.setTxPower(WIFI_RUN_TXPOWER);
        WiFi.setSleep(false);
        // Wi-Fi up; print reminder of server endpoint
        LOG_INFO("HTTP server ready on port 80");
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
  static uint32_t lastLvgl = 0;
  static bool lastTouch = false;
  static uint32_t lastSeq = 0;
  static uint32_t lastBattUi = 0;

  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  uint32_t now = millis();
  if (g_displayReady) {
    bool touched = g_panel.isPressed();
    if (touched != lastTouch) {
      LOG_INFO("Touch %s", touched ? "ON" : "OFF");
      lastTouch = touched;
    }
    if (touched) {
      g_lastTouchMs = now;
      if (TOUCH_BRIGHTNESS_MS > 0) {
        g_touchBoostUntilMs = now + TOUCH_BRIGHTNESS_MS;
      }
    }
    bool touchBoost = g_touchBoostUntilMs != 0 && (int32_t)(g_touchBoostUntilMs - now) > 0;
    bool closeBoost = g_haveDisplayed && !isnan(g_lastShown.distanceKm) &&
                      g_lastShown.distanceKm <= CLOSE_RADIUS_KM;
    bool milBoost = g_haveDisplayed && g_lastShown.opClass == "MIL";
    uint8_t target = clampBrightness((touchBoost || closeBoost || milBoost) ? TOUCH_BRIGHTNESS_MAX
                                                                : TOUCH_BRIGHTNESS_MIN);
    if (g_lastBrightness != target) {
      g_panel.setBrightness(target);
      g_lastBrightness = target;
    }
  }

  if (g_displayReady && BATTERY_UI_UPDATE_MS > 0) {
    if ((int32_t)(now - lastBattUi) >= (int32_t)BATTERY_UI_UPDATE_MS) {
      lastBattUi = now;
      updateBatteryUi();
    }
  }

  if (g_displayReady && TOUCH_IDLE_SLEEP_MS > 0) {
    bool charging = g_panel.hasPowerManagement() && g_panel.isCharging();
    if (!charging && (int32_t)(now - g_lastTouchMs) >= (int32_t)TOUCH_IDLE_SLEEP_MS) {
      LOG_INFO("Idle timeout reached; entering deep sleep");
      g_panel.enableTouchWakeup();
      g_panel.sleep();
    }
  }

  if (digitalRead(SLEEP_BUTTON_PIN) == LOW) {
    if (g_sleepHoldStartMs == 0) g_sleepHoldStartMs = now;
    if ((int32_t)(now - g_sleepHoldStartMs) >= (int32_t)SLEEP_HOLD_MS) {
      LOG_INFO("Sleep button held; entering deep sleep");
      g_panel.enableButtonWakeup();
      // Avoid immediate wake if the button is still held low.
      uint32_t releaseStart = millis();
      while (digitalRead(SLEEP_BUTTON_PIN) == LOW) {
        if ((int32_t)(millis() - releaseStart) > 2000) {
          LOG_WARN("Sleep aborted; button still held");
          g_sleepHoldStartMs = 0;
          return;
        }
        waitMs(20);
      }
      g_panel.sleep();
    }
  } else {
    g_sleepHoldStartMs = 0;
  }

#if FEATURE_TEST_ENDPOINT
  if (g_httpStarted) {
    g_http.handleClient();
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

  uint32_t seq = 0;
  bool pendingValid = false;
  FlightInfo pending;
  portENTER_CRITICAL(&g_flightMux);
  seq = g_pendingSeq;
  pendingValid = g_pendingValid;
  if (pendingValid) {
    pending = g_pendingFlight;
  }
  portEXIT_CRITICAL(&g_flightMux);
  if (seq != lastSeq) {
    lastSeq = seq;
    if (pendingValid) {
      if (!g_haveDisplayed || !sameFlightDisplay(pending, g_lastShown)) {
        renderFlight(pending);
        g_lastShown = pending;
        g_haveDisplayed = true;
      }
    } else if (!g_haveDisplayed) {
      renderNoData("Check Wi-Fi/API");
    }
  }

  if (g_lvReady) {
    if (now - lastLvgl >= 5) {
      lv_timer_handler();
      lastLvgl = now;
    }
  }

  // Cooperative yield without blocking
  yield();
}
