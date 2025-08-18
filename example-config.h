// Copy this file to config.h and adjust for your setup.
// Do NOT commit config.h with real credentials.

#pragma once

// Wi‑Fi
#define WIFI_SSID "SSID"
#define WIFI_PASSWORD "PASSWORD"

// Location and search radius (km)
#define HOME_LAT 00.0000
#define HOME_LON -000.0000
#define SEARCH_RADIUS_KM 10

// API base (https enabled)
#define API_BASE "https://api.adsb.lol"

// OLED hardware (SSD1322 SPI)
#define SCREEN_WIDTH 256
#define SCREEN_HEIGHT 64
// SPI pins for the OLED
#define PIN_CS 5     // /CS
#define PIN_DC 16    // D/C
#define PIN_RST 17   // /RES
#define PIN_CLK 18   // SCLK
#define PIN_MOSI 23  // SDIN

// 4-channel relay module GPIO (active level configured in sketch)
#define RELAY_IN1_PIN 33
#define RELAY_IN2_PIN 25
#define RELAY_IN3_PIN 26
#define RELAY_IN4_PIN 27
