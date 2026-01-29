// ESP32 ADS-B Flight Display
// - Connects to Wi-Fi
// - Calls adsb.lol /v2/lat/{lat}/lon/{lon}/dist/{radius}
// - Parses nearest aircraft and renders summary on a 466x466 round AMOLED

#include <Arduino.h>
#include <lvgl.h>

#include "app_config.h"
#include "app_controller.h"
#include "display/drivers/common/LV_Helper.h"
#include "display_init.h"
#include "log.h"
#include "networking.h"
#include "ui.h"

static void waitMs(uint32_t durationMs) {
  uint32_t start = millis();
  while ((int32_t)(millis() - start) < (int32_t)durationMs) {
    yield();
  }
}

void setup() {
  Serial.begin(115200);
  waitMs(20);
  LOG_INFO("Boot: Flight Display starting...");

  if (!displayInit()) {
    while (true) {
      LOG_ERROR("Display init failed");
      delay(1000);
    }
  }

  beginLvglHelper(displayPanel(), false);
  DisplayMetrics metrics = displayGetMetrics();
  UiState ui = uiInit(metrics);
  if (ui.ready) {
    uiRenderSplash(ui, "Booting...", nullptr);
  }

  networkingInit();
  networkingStartFetchTask();
  appControllerInit(ui);
}

void loop() {
  appControllerTick();
}
