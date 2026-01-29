#include "display_init.h"

#include <Arduino.h>
#include <algorithm>

#include "app_config.h"
#include "config_hw.h"
#include "log.h"
#include "display/drivers/AmoledDisplay/Amoled_DisplayPanel.h"
#include "display/drivers/AmoledDisplay/pin_config.h"

#if defined(ESP32)
#include <esp_system.h>
#endif

#if AMOLED_PANEL_WAVESHARE
constexpr AmoledHwConfig kHwConfig = WAVESHARE_S3_AMOLED_HW_CONFIG;
#else
constexpr AmoledHwConfig kHwConfig = LILYGO_T_DISPLAY_S3_DS_HW_CONFIG;
#endif

static Amoled_DisplayPanel g_panel(kHwConfig);
static Arduino_GFX *g_gfx = nullptr;
static DisplayState g_state;

static uint32_t backoffMs(uint8_t attempt, uint32_t base = 350, uint32_t cap = 6000) {
  uint32_t exp = base << std::min<uint8_t>(attempt, 5);
  uint32_t j = (exp >> 3) * (esp_random() & 0x7) / 7;  // ~±12.5% jitter
  return std::min(cap, exp - (exp >> 4) + j);
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

static void updateMetrics() {
  g_state.metrics.screenW = g_panel.width();
  g_state.metrics.screenH = g_panel.height();
  g_state.metrics.centerX = g_state.metrics.screenW / 2;
  g_state.metrics.centerY = g_state.metrics.screenH / 2;
  g_state.metrics.safeRadius =
      std::min(g_state.metrics.centerX, g_state.metrics.centerY) - 18;
}

static bool initDisplayFresh() {
  for (uint8_t attempt = 0; attempt < 4; ++attempt) {
    if (g_panel.begin(AMOLED_COLOR_ORDER)) {
      g_panel.setBrightness(clampBrightness(TOUCH_BRIGHTNESS_MIN));
      g_state.brightness = clampBrightness(TOUCH_BRIGHTNESS_MIN);
      g_gfx = g_panel.gfx();
      updateMetrics();
      g_gfx->fillScreen(g_gfx->color565(6, 7, 8));
      g_state.ready = true;
      return true;
    }
    waitMs(backoffMs(attempt));
  }
  return false;
}

static bool initDisplayFromPanelReady() {
  g_gfx = g_panel.gfx();
  if (!g_gfx) return false;
  updateMetrics();
  g_gfx->fillScreen(g_gfx->color565(6, 7, 8));
  g_state.ready = true;
  g_state.brightness = clampBrightness(g_panel.getBrightness());
  return true;
}

bool displayInit() {
  g_state = DisplayState{};

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

  if (wokeFromSleep) {
    bool displayOk = g_panel.wakeup();
    if (displayOk) {
      displayOk = initDisplayFromPanelReady();
      if (displayOk) {
        g_panel.setBrightness(clampBrightness(TOUCH_BRIGHTNESS_MIN));
        g_state.brightness = clampBrightness(TOUCH_BRIGHTNESS_MIN);
        LOG_INFO("Display wakeup complete");
        return true;
      }
    }
  }
#endif

  return initDisplayFresh();
}

DisplayState displayGetState() { return g_state; }

DisplayMetrics displayGetMetrics() { return g_state.metrics; }

Amoled_DisplayPanel &displayPanel() { return g_panel; }

Arduino_GFX *displayGfx() { return g_gfx; }

bool displayIsReady() { return g_state.ready; }

void displaySetBrightness(uint8_t level) {
  uint8_t clamped = clampBrightness(level);
  g_panel.setBrightness(clamped);
  g_state.brightness = clamped;
}
