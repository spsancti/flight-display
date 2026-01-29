#pragma once

#include "app_types.h"

struct AppControllerState {
  UiState ui;
  FlightInfo lastShown;
  bool haveDisplayed = false;
  uint32_t lastSeq = 0;
  uint32_t lastBattUi = 0;
  uint32_t lastLvglMs = 0;
};

void appControllerInit(const UiState &ui);
void appControllerTick();
