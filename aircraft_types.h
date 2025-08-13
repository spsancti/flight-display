// Aircraft type code to friendly name lookup
// Keep this table concise and focused on common GA and airliner codes.

#pragma once

#include <Arduino.h>

// Rich dataset: ICAO/IATA/manufacturer/model/seat range
struct AircraftTypeInfo {
  const char* icao;
  const char* iata;  // may be empty
  const char* manufacturer;
  const char* model;
  uint16_t minSeats;
  uint16_t maxSeats;
};

static const AircraftTypeInfo kTypeInfo[] = {
    {"BCS1", "221", "Airbus", "A220-100", 108, 135},
    {"BCS3", "223", "Airbus", "A220-300", 130, 160},
    {"A318", "318", "Airbus", "A318", 107, 107},
    {"A319", "319", "Airbus", "A319", 124, 156},
    {"A320", "320", "Airbus", "A320", 150, 186},
    {"A321", "321", "Airbus", "A321", 185, 236},
    {"A332", "332", "Airbus", "A330-200", 210, 272},
    {"A333", "333", "Airbus", "A330-300", 277, 440},
    {"A343", "343", "Airbus", "A340-300", 295, 335},
    {"A346", "346", "Airbus", "A340-600", 326, 440},
    {"A359", "359", "Airbus", "A350-900", 300, 350},
    {"A35K", "351", "Airbus", "A350-1000", 350, 410},
    {"A388", "388", "Airbus", "A380-800", 555, 615},
    {"B712", "717", "Boeing", "717-200", 110, 110},
    {"B737", "73G", "Boeing", "737-700", 126, 149},
    {"B738", "738", "Boeing", "737-800", 162, 189},
    {"B38M", "7M8", "Boeing", "737 MAX 8", 162, 210},
    {"B39M", "7M9", "Boeing", "737 MAX 9", 178, 220},
    {"B744", "744", "Boeing", "747-400", 416, 524},
    {"B748", "748", "Boeing", "747-8", 410, 524},
    {"B752", "752", "Boeing", "757-200", 200, 239},
    {"B753", "753", "Boeing", "757-300", 243, 280},
    {"B763", "763", "Boeing", "767-300", 211, 269},
    {"B764", "764", "Boeing", "767-400", 245, 375},
    {"B772", "772", "Boeing", "777-200", 314, 396},
    {"B773", "773", "Boeing", "777-300", 368, 451},
    {"B788", "788", "Boeing", "787-8", 242, 290},
    {"B789", "789", "Boeing", "787-9", 280, 335},
    {"B78X", "78J", "Boeing", "787-10", 318, 440},
    {"E170", "E70", "Embraer", "E170", 66, 78},
    {"E175", "E75", "Embraer", "E175", 76, 88},
    {"E190", "E90", "Embraer", "E190", 96, 114},
    {"E195", "E95", "Embraer", "E195", 108, 124},
    {"CRJ2", "CR2", "Bombardier", "CRJ-200", 50, 50},
    {"CRJ7", "CR7", "Bombardier", "CRJ-700", 66, 78},
    {"CRJ9", "CR9", "Bombardier", "CRJ-900", 76, 90},
    {"CRJX", "CRK", "Bombardier", "CRJ-1000", 90, 104},
    {"AT45", "ATR", "ATR", "ATR 42-500", 42, 50},
    {"AT76", "AT7", "ATR", "ATR 72-600", 68, 78},
    {"C172", "", "Cessna", "172 Skyhawk", 4, 4},
    {"C182", "", "Cessna", "182 Skylane", 4, 4},
    {"C208", "", "Cessna", "208 Caravan", 9, 14},
    {"C208A", "", "Cessna", "208 Caravan Amphibian", 9, 14},
    {"P28A", "", "Piper", "PA-28 Cherokee", 2, 4},
    {"PA34", "", "Piper", "PA-34 Seneca", 6, 6},
    {"BE35", "", "Beechcraft", "Bonanza", 4, 6},
    {"B350", "", "Beechcraft", "King Air 350", 8, 11},
    {"SR22", "", "Cirrus", "SR22", 4, 4},
    {"DHC2", "", "de Havilland Canada", "DHC-2 Beaver", 6, 7},
    {"DHC3", "", "de Havilland Canada", "DHC-3 Otter", 10, 14},
    {"DH6T", "", "de Havilland Canada", "DHC-6 Twin Otter", 19, 19},
    {"DHC7", "", "de Havilland Canada", "DHC-7 Dash 7", 40, 50},
    {"PC12", "", "Pilatus", "PC-12", 6, 9},
    {"KODI", "", "Quest", "Kodiak 100", 9, 10},
    {"DHC2T", "", "Viking Air", "DHC-2T Turbo Beaver", 6, 7},
    {"DH6T4", "", "Viking Air", "DHC-6-400 Twin Otter", 19, 19},
    {"CL415", "", "Viking Air", "CL-415 Amphibian", 6, 6},
    {"HU16", "", "Grumman", "HU-16 Albatross", 10, 14},
    {"BE200", "", "Beriev", "Be-200 Altair", 72, 72},
    {"LA4",  "", "Lake Aircraft", "LA-4-200 Buccaneer", 3, 4},
    {"ICON", "", "ICON", "Icon A5", 2, 2},
    {"AS50", "", "Airbus Helicopters", "H125/AS350 Ecureuil", 6, 6},
    {"EC30", "", "Airbus Helicopters", "H130/EC130", 7, 7},
    {"EC45", "", "Airbus Helicopters", "H145/EC145", 8, 10},
    {"B06",  "", "Bell", "206 JetRanger", 4, 5},
    {"B407", "", "Bell", "407", 6, 6},
    {"B412", "", "Bell", "412", 13, 15},
    {"B429", "", "Bell", "429", 7, 8},
    {"R22",  "", "Robinson", "R22", 2, 2},
    {"R44",  "", "Robinson", "R44", 4, 4},
    {"R66",  "", "Robinson", "R66", 4, 5},
    {"H60",  "", "Sikorsky", "UH-60 Black Hawk", 11, 14},
    {"S76",  "", "Sikorsky", "S-76", 12, 12},
    {"S92",  "", "Sikorsky", "S-92", 19, 19},
    {"AW109","", "AgustaWestland", "AW109", 6, 7},
    {"AW139","", "AgustaWestland", "AW139", 15, 15},
    {"C130", "", "Lockheed Martin", "C-130 Hercules", 64, 92},
    {"C5M",  "", "Lockheed Martin", "C-5 Galaxy", 75, 270},
    {"C17",  "", "Boeing", "C-17 Globemaster III", 102, 102},
    {"K35R", "", "Boeing", "KC-135 Stratotanker", 0, 0},
    {"P8",   "", "Boeing", "P-8 Poseidon", 9, 9},
    {"B2",   "", "Northrop Grumman", "B-2 Spirit", 2, 2},
    {"F15",  "", "McDonnell Douglas", "F-15 Eagle", 1, 2},
    {"F16",  "", "General Dynamics", "F-16 Fighting Falcon", 1, 1},
    {"F35",  "", "Lockheed Martin", "F-35 Lightning II", 1, 1},
    {"EUFI", "", "Eurofighter", "Typhoon", 1, 2},
};

struct AircraftTypeEntry {
  const char* code;  // ICAO type designator or common shorthand
  const char* name;  // Friendly name
};

static const AircraftTypeEntry kAircraftTypes[] = {
  { "C152", "Cessna 152" },
  { "C172", "Cessna 172 Skyhawk" },
  { "C182", "Cessna 182 Skylane" },
  { "C206", "Cessna 206 Stationair" },
  { "C208", "Cessna 208 Caravan" },
  { "C210", "Cessna 210 Centurion" },
  { "SR20", "Cirrus SR20" },
  { "SR22", "Cirrus SR22" },
  { "PA28", "Piper PA-28" },
  { "PA32", "Piper Saratoga" },
  { "C56X", "Citation Excel/XLS" },
  { "GLF4", "Gulfstream IV" },
  { "GLF5", "Gulfstream V" },
  { "B737", "Boeing 737" },
  { "B738", "Boeing 737-800" },
  { "B739", "Boeing 737-900" },
  { "B38M", "Boeing 737 MAX 8" },
  { "B39M", "Boeing 737 MAX 9" },
  { "B752", "Boeing 757-200" },
  { "B753", "Boeing 757-300" },
  { "B763", "Boeing 767-300" },
  { "B772", "Boeing 777-200" },
  { "B773", "Boeing 777-300" },
  { "B788", "Boeing 787-8" },
  { "A319", "Airbus A319" },
  { "A320", "Airbus A320" },
  { "A321", "Airbus A321" },
  { "A20N", "Airbus A320neo" },
  { "A21N", "Airbus A321neo" },
  { "BCS1", "Airbus A220-100" },
  { "BCS3", "Airbus A220-300" },
  { "A221", "Airbus A220-100" },
  { "A223", "Airbus A220-300" },
  { "E170", "Embraer 170" },
  { "E175", "Embraer 175" },
  { "E190", "Embraer 190" },
  { "E195", "Embraer 195" },
  { "CRJ2", "CRJ200" },
  { "CRJ7", "CRJ700" },
  { "CRJ9", "CRJ900" },
  { "DH8D", "Dash 8 Q400" },
};

inline String aircraftFriendlyName(const String& rawCode) {
  if (rawCode.length() == 0) return String("");
  String code = rawCode;
  code.trim();
  code.toUpperCase();
  // Prefer full info table (match ICAO first, then IATA)
  for (size_t i = 0; i < sizeof(kTypeInfo) / sizeof(kTypeInfo[0]); ++i) {
    if (code.equals(kTypeInfo[i].icao) || (kTypeInfo[i].iata && strlen(kTypeInfo[i].iata) && code.equals(kTypeInfo[i].iata))) {
      return String(kTypeInfo[i].manufacturer) + " " + String(kTypeInfo[i].model);
    }
  }
  for (size_t i = 0; i < sizeof(kAircraftTypes) / sizeof(kAircraftTypes[0]); ++i) {
    if (code.equals(kAircraftTypes[i].code)) {
      return String(kAircraftTypes[i].name);
    }
  }
  // Some families: map by prefix for broader coverage
  if (code.startsWith("B77")) return String("Boeing 777");
  if (code.startsWith("B78")) return String("Boeing 787");
  if (code.startsWith("A32")) return String("Airbus A320 family");
  if (code.startsWith("B74")) return String("Boeing 747");
  if (code.startsWith("B76")) return String("Boeing 767");
  if (code.startsWith("B75")) return String("Boeing 757");
  return String("");
}

inline bool aircraftSeatRange(const String& rawCode, uint16_t& minOut, uint16_t& maxOut) {
  String code = rawCode; code.trim(); code.toUpperCase();
  for (size_t i = 0; i < sizeof(kTypeInfo) / sizeof(kTypeInfo[0]); ++i) {
    if (code.equals(kTypeInfo[i].icao) || (kTypeInfo[i].iata && strlen(kTypeInfo[i].iata) && code.equals(kTypeInfo[i].iata))) {
      minOut = kTypeInfo[i].minSeats; maxOut = kTypeInfo[i].maxSeats; return true;
    }
  }
  return false;
}

inline String aircraftDisplayType(const String& rawCode) {
  String code = rawCode;
  code.trim();
  String name = aircraftFriendlyName(code);
  if (name.length() == 0) return code;  // fallback to raw code
  if (code.length() == 0) return name;  // rare, but handle gracefully
  return code + " " + name;             // e.g., "C208 Cessna 208 Caravan"
}
