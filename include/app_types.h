#pragma once

#include <Arduino.h>

struct FlightInfo {
  String ident;            // flight/callsign or registration/hex fallback
  String typeCode;         // aircraft type (t)
  String category;         // raw category code
  String displayName;      // optional override (e.g., hexdb)
  String registeredOwner;  // HexDB RegisteredOwners fallback
  long altitudeFt = -1;
  double lat = NAN;
  double lon = NAN;
  double distanceKm = NAN;
  String hex;          // transponder hex id
  bool hasCallsign = false;
  String opClass;      // MIL/COM/PVT
  String route;        // route string (e.g., TLV-RMO)
  bool valid = false;
  int seatOverride = -1;  // if >0, override seat display
};

struct DisplayMetrics {
  int16_t screenW = 0;
  int16_t screenH = 0;
  int16_t centerX = 0;
  int16_t centerY = 0;
  int16_t safeRadius = 0;
};

struct DisplayState {
  DisplayMetrics metrics;
  bool ready = false;
  uint8_t brightness = 0;
};

struct UiState {
  bool ready = false;
};
