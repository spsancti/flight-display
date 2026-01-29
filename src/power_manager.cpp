#include "power_manager.h"

#include <Arduino.h>

#include "config_hw.h"
#include "display_init.h"
#include "log.h"

static uint8_t clampBrightness(int value) {
  if (value < 1) return 1;
  if (value > 16) return 16;
  return static_cast<uint8_t>(value);
}

static void waitMs(uint32_t durationMs) {
  uint32_t start = millis();
  while ((int32_t)(millis() - start) < (int32_t)durationMs) {
    yield();
  }
}

void powerManagerInit(PowerManagerState &state) {
  pinMode(SLEEP_BUTTON_PIN, INPUT_PULLUP);
  state.lastTouchMs = millis();
  state.lastBrightness = displayGetState().brightness;
}

void powerManagerTick(PowerManagerState &state, const FlightInfo *lastShown) {
  if (!displayIsReady()) return;

  uint32_t now = millis();
  bool touched = displayPanel().isPressed();
  if (touched != state.lastTouch) {
    LOG_INFO("Touch %s", touched ? "ON" : "OFF");
    state.lastTouch = touched;
  }
  if (touched) {
    state.lastTouchMs = now;
    if (TOUCH_BRIGHTNESS_MS > 0) {
      state.touchBoostUntilMs = now + TOUCH_BRIGHTNESS_MS;
    }
  }

  bool touchBoost = state.touchBoostUntilMs != 0 &&
                    (int32_t)(state.touchBoostUntilMs - now) > 0;
  bool closeBoost = lastShown && !isnan(lastShown->distanceKm) &&
                    lastShown->distanceKm <= CLOSE_RADIUS_KM;
  bool milBoost = lastShown && lastShown->opClass == "MIL";

  uint8_t target = clampBrightness((touchBoost || closeBoost || milBoost)
                                       ? TOUCH_BRIGHTNESS_MAX
                                       : TOUCH_BRIGHTNESS_MIN);
  if (state.lastBrightness != target) {
    displaySetBrightness(target);
    state.lastBrightness = target;
  }

  if (TOUCH_IDLE_SLEEP_MS > 0) {
    bool charging = displayPanel().hasPowerManagement() && displayPanel().isCharging();
    if (!charging && (int32_t)(now - state.lastTouchMs) >= (int32_t)TOUCH_IDLE_SLEEP_MS) {
      LOG_INFO("Idle timeout reached; entering deep sleep");
      displayPanel().enableTouchWakeup();
      displayPanel().sleep();
    }
  }

  if (digitalRead(SLEEP_BUTTON_PIN) == LOW) {
    if (state.sleepHoldStartMs == 0) state.sleepHoldStartMs = now;
    if ((int32_t)(now - state.sleepHoldStartMs) >= (int32_t)SLEEP_HOLD_MS) {
      LOG_INFO("Sleep button held; entering deep sleep");
      displayPanel().enableButtonWakeup();
      uint32_t releaseStart = millis();
      while (digitalRead(SLEEP_BUTTON_PIN) == LOW) {
        if ((int32_t)(millis() - releaseStart) > 2000) {
          LOG_WARN("Sleep aborted; button still held");
          state.sleepHoldStartMs = 0;
          return;
        }
        waitMs(20);
      }
      displayPanel().sleep();
    }
  } else {
    state.sleepHoldStartMs = 0;
  }
}
