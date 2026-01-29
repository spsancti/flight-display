#pragma once

#include "app_types.h"

struct PowerManagerState {
  uint32_t lastTouchMs = 0;
  uint32_t touchBoostUntilMs = 0;
  uint32_t sleepHoldStartMs = 0;
  uint8_t lastBrightness = 0;
  bool lastTouch = false;
};

void powerManagerInit(PowerManagerState &state);
void powerManagerTick(PowerManagerState &state, const FlightInfo *lastShown);
