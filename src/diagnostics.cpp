#include "diagnostics.h"

#include <Arduino.h>

#include "config_features.h"
#include "log.h"

#ifndef DIAGNOSTICS_INTERVAL_MS
#define DIAGNOSTICS_INTERVAL_MS 60000
#endif

static uint32_t g_lastLogMs = 0;

void diagnosticsInit() {
  g_lastLogMs = millis();
}

void diagnosticsTick() {
#if FEATURE_DIAGNOSTICS
  uint32_t now = millis();
  if ((int32_t)(now - g_lastLogMs) >= (int32_t)DIAGNOSTICS_INTERVAL_MS) {
    g_lastLogMs = now;
#if defined(ESP32)
    LOG_INFO("Heap free=%u min=%u", (unsigned)ESP.getFreeHeap(),
             (unsigned)ESP.getMinFreeHeap());
#else
    LOG_INFO("Diagnostics tick");
#endif
  }
#endif
}
