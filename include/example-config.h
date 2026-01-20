// Copy this file to config.h and adjust for your setup.
// Do NOT commit config.h with real credentials.

#pragma once

// Wi-Fi
#define WIFI_SSID "SSID"
#define WIFI_PASSWORD "PASSWORD"

// Location and search radius (km)
#define HOME_LAT 00.0000
#define HOME_LON -000.0000
#define SEARCH_RADIUS_KM 10

// API base (https enabled)
#define API_BASE "https://api.adsb.lol"

// AMOLED panel selection
// Set exactly one of these to 1.
#define AMOLED_PANEL_LILYGO 1
#define AMOLED_PANEL_WAVESHARE 0

// AMOLED display tuning
#define AMOLED_BRIGHTNESS 12  // 1..16
// #define AMOLED_COLOR_ORDER ORDER_RGB

// --- OTA (ArduinoOTA) ---
// Set FEATURE_OTA to 1 to enable over-the-air updates.
// Provide a strong OTA password for production.
// #define FEATURE_OTA 1
// #define OTA_HOSTNAME "flight-display"
// #define OTA_PORT 3232
// #define OTA_PASSWORD "change-me"
