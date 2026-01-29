#include "ui.h"

#include <Arduino.h>
#include <lvgl.h>
#include <math.h>

#include "aircraft_types.h"
#include "display_init.h"
#include "log.h"


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
static DisplayMetrics g_metrics;

static int16_t g_windowX = 0;
static int16_t g_windowY = 0;
static int16_t g_windowW = 0;
static int16_t g_windowH = 0;
static int16_t g_labelY = 0;

static void computeLayout() {
  g_windowW = (int16_t)(g_metrics.screenW * 0.90f) - 20;
  g_windowH = (int16_t)(g_metrics.screenH * 0.42f);
  if (g_windowW < 200) g_windowW = 200;
  if (g_windowH < 140) g_windowH = 140;
  int16_t maxW = g_metrics.safeRadius * 2 - 8;
  int16_t maxH = g_metrics.safeRadius * 2 - 120;
  if (g_windowW > maxW) g_windowW = maxW;
  if (g_windowH > maxH) g_windowH = maxH;
  g_windowX = g_metrics.centerX - g_windowW / 2;
  g_windowY = g_metrics.centerY - g_windowH / 2 - 10;
  g_labelY = g_windowY + g_windowH + 18;
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
}

static void uiSetMetrics(const char *dist, const char *seats, const char *alt) {
  if (!g_lvReady) return;
  lv_label_set_text(g_lv.metricVal[0], dist);
  lv_label_set_text(g_lv.metricVal[1], seats);
  lv_label_set_text(g_lv.metricVal[2], alt);
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
  lv_obj_set_style_text_color(g_lv.battLbl,
                              charging ? g_lvColors.mil : g_lvColors.green,
                              LV_PART_MAIN);
}

UiState uiInit(const DisplayMetrics &metrics) {
  g_metrics = metrics;
  UiState state;

  if (!displayIsReady()) {
    return state;
  }
  computeLayout();

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

  int16_t d = min(g_metrics.screenW, g_metrics.screenH) - 8;
  g_lv.bezel = lv_obj_create(scr);
  lv_obj_set_size(g_lv.bezel, d, d);
  lv_obj_set_style_radius(g_lv.bezel, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_lv.bezel, g_lvColors.bezel, LV_PART_MAIN);
  lv_obj_set_style_border_color(g_lv.bezel, g_lvColors.bezelBorder, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_lv.bezel, 2, LV_PART_MAIN);
  lv_obj_clear_flag(g_lv.bezel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(g_lv.bezel, g_metrics.centerX - d / 2, g_metrics.centerY - d / 2);

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
  lv_obj_set_style_text_color(g_lv.subtitle, g_lvColors.green, LV_PART_MAIN);
  lv_obj_set_style_text_font(g_lv.subtitle, &lv_font_montserrat_24, LV_PART_MAIN);
  lv_obj_set_style_text_align(g_lv.subtitle, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_align(g_lv.subtitle, LV_ALIGN_TOP_MID, 0, 8);

  g_lv.route = lv_label_create(g_lv.window);
  lv_label_set_long_mode(g_lv.route, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(g_lv.route, g_windowW - 16);
  lv_obj_set_style_text_color(g_lv.route, g_lvColors.green, LV_PART_MAIN);
  lv_obj_set_style_text_font(g_lv.route, &lv_font_montserrat_24, LV_PART_MAIN);
  lv_obj_set_style_text_align(g_lv.route, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_align(g_lv.route, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_label_set_text(g_lv.route, "-");

  static const char *kMetricLabels[3] = {"DIST", "SOULS", "ALT"};
  for (int i = 0; i < 3; ++i) {
    g_lv.metricLbl[i] = lv_label_create(scr);
    lv_label_set_text(g_lv.metricLbl[i], kMetricLabels[i]);
    lv_obj_set_style_text_color(g_lv.metricLbl[i], g_lvColors.label, LV_PART_MAIN);
    lv_obj_set_style_text_font(g_lv.metricLbl[i], &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(g_lv.metricLbl[i], 2, LV_PART_MAIN);
  }

  const float anglesDeg[3] = {238.0f, 270.0f, 302.0f};
  int16_t r = g_metrics.safeRadius - 8;
  for (int i = 0; i < 3; ++i) {
    float radians = anglesDeg[i] * PI / 180.0f;
    int16_t x = g_metrics.centerX + (int16_t)(cosf(radians) * r);
    int16_t y = g_metrics.centerY + (int16_t)(sinf(radians) * r);
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
    int16_t startX = g_metrics.centerX - totalW / 2;
    int16_t midY = lv_obj_get_y(g_lv.metricVal[1]) + lv_obj_get_height(g_lv.metricVal[1]) +
                   6 + (btnH / 2);
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
    int16_t topY = g_metrics.centerY - g_metrics.safeRadius + 18;
    lv_obj_set_pos(g_lv.timeLbl, g_metrics.centerX - 100, topY);
    lv_obj_set_pos(g_lv.battLbl, g_metrics.centerX + 30, topY);
  }

  g_lvReady = true;
  state.ready = true;
  return state;
}

void uiUpdateBattery(const UiState &state) {
  if (!state.ready || !displayIsReady()) return;
  uint16_t mv = displayPanel().getBattVoltage();
  bool charging = displayPanel().isCharging();
  uiSetBatteryMv(mv, charging);
}

void uiRenderSplash(const UiState &state, const char *title, const char *subtitle) {
  if (!state.ready || !displayIsReady()) return;
  uiSetOpClass(nullptr);
  uiSetTitle(String(title ? title : ""), subtitle ? String(subtitle) : String(""));
  uiSetRoute(String("-"));
  uiSetMetrics("-", "-", "-");
}

void uiRenderNoData(const UiState &state, const char *detail) {
  if (!state.ready || !displayIsReady()) return;
  uiSetOpClass(nullptr);
  uiSetTitle(String("No Data"), detail ? String(detail) : String(""));
  uiSetRoute(String("-"));
  uiSetMetrics("-", "-", "-");
}

void uiRenderFlight(const UiState &state, const FlightInfo &fi) {
  if (!state.ready || !displayIsReady()) return;

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
  if (!friendly.length() && fi.displayName.length()) friendly = fi.displayName;
  if (!friendly.length()) friendly = String("Unknown Aircraft");

  uiSetOpClass(fi.opClass.c_str());

  String callsign = fi.ident.length() ? fi.ident : String("-");
  uiSetTitle(friendly, callsign);

  String routeLine = fi.route;
  if (!routeLine.length() && fi.registeredOwner.length()) {
    routeLine = fi.registeredOwner;
  }
  if (!routeLine.length()) routeLine = String("-");
  uiSetRoute(routeLine);

  char distStr[16];
  if (!isnan(fi.distanceKm)) {
    snprintf(distStr, sizeof(distStr), "%.1f km", fi.distanceKm);
  } else {
    snprintf(distStr, sizeof(distStr), "-");
  }

  char seatsStr[12];
  if (isPseudo) {
    snprintf(seatsStr, sizeof(seatsStr), "-");
  } else if (fi.seatOverride > 0) {
    snprintf(seatsStr, sizeof(seatsStr), "%d", fi.seatOverride);
  } else {
    uint16_t maxSeats = 0;
    if (fi.typeCode.length() && aircraftSeatMax(fi.typeCode, maxSeats) && maxSeats > 0) {
      snprintf(seatsStr, sizeof(seatsStr), "%u", maxSeats);
    } else {
      snprintf(seatsStr, sizeof(seatsStr), "-");
    }
  }

  char altStr[16];
  if (fi.altitudeFt <= 0) {
    snprintf(altStr, sizeof(altStr), "ground");
  } else {
    int meters = (int)(fi.altitudeFt * 0.3048 + 0.5);
    snprintf(altStr, sizeof(altStr), "%d m", meters);
  }

  uiSetMetrics(distStr, seatsStr, altStr);
}

bool uiIsReady(const UiState &state) { return state.ready; }
