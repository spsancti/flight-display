#include "app_controller.h"

#include <Arduino.h>
#include <math.h>
#include <lvgl.h>

#include "config_features.h"
#include "config_hw.h"
#include "diagnostics.h"
#include "display_init.h"
#include "log.h"
#include "networking.h"
#include "power_manager.h"
#include "ui.h"

static AppControllerState g_state;
static PowerManagerState g_power;

static bool sameFlightDisplay(const FlightInfo &a, const FlightInfo &b) {
  if (!a.valid && !b.valid) return true;
  if (a.valid != b.valid) return false;
  if (a.ident != b.ident) return false;
  if (a.typeCode != b.typeCode) return false;
  if (a.altitudeFt != b.altitudeFt) return false;
  if (a.opClass != b.opClass) return false;
  if (a.route != b.route) return false;
  double da = isnan(a.distanceKm) ? 0 : a.distanceKm;
  double db = isnan(b.distanceKm) ? 0 : b.distanceKm;
  if (fabs(da - db) > 0.1) return false;
  return true;
}

void appControllerInit(const UiState &ui) {
  g_state = AppControllerState{};
  g_state.ui = ui;
  powerManagerInit(g_power);
  diagnosticsInit();
}

void appControllerTick() {
  networkingEnsureConnected();

  uint32_t now = millis();

  if (displayIsReady()) {
    powerManagerTick(g_power, g_state.haveDisplayed ? &g_state.lastShown : nullptr);
  }

  if (displayIsReady() && uiIsReady(g_state.ui) && BATTERY_UI_UPDATE_MS > 0) {
    if ((int32_t)(now - g_state.lastBattUi) >= (int32_t)BATTERY_UI_UPDATE_MS) {
      g_state.lastBattUi = now;
      uiUpdateBattery(g_state.ui);
    }
  }

  uint32_t seq = 0;
  bool pendingValid = false;
  FlightInfo pending;
  networkingGetLatest(pending, pendingValid, seq);

  if (seq != g_state.lastSeq) {
    g_state.lastSeq = seq;
    if (pendingValid) {
      if (!g_state.haveDisplayed || !sameFlightDisplay(pending, g_state.lastShown)) {
        uiRenderFlight(g_state.ui, pending);
        g_state.lastShown = pending;
        g_state.haveDisplayed = true;
      }
    } else if (!g_state.haveDisplayed) {
      uiRenderNoData(g_state.ui, "Check Wi-Fi/API");
    }
  }

  if (uiIsReady(g_state.ui)) {
    if (now - g_state.lastLvglMs >= 5) {
      lv_timer_handler();
      g_state.lastLvglMs = now;
    }
  }

  diagnosticsTick();
  yield();
}
