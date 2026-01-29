#pragma once

#include <WiFi.h>
#include <Arduino_GFX_Library.h>

#ifndef BOOT_POWER_SETTLE_MS
#define BOOT_POWER_SETTLE_MS 1200
#endif

#ifndef WIFI_BOOT_TXPOWER
#define WIFI_BOOT_TXPOWER WIFI_POWER_8_5dBm
#endif

#ifndef WIFI_RUN_TXPOWER
#define WIFI_RUN_TXPOWER WIFI_POWER_15dBm
#endif

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
#define AMOLED_BRIGHTNESS 12
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
