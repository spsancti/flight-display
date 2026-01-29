#pragma once

#include "app_types.h"

UiState uiInit(const DisplayMetrics &metrics);
void uiUpdateBattery(const UiState &state);
void uiRenderSplash(const UiState &state, const char *title, const char *subtitle);
void uiRenderNoData(const UiState &state, const char *detail);
void uiRenderFlight(const UiState &state, const FlightInfo &fi);
bool uiIsReady(const UiState &state);
