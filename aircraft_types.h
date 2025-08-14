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
    // Airbus A220 family
    {"BCS1", "221", "Airbus", "A220-100", 108, 135},
    {"BCS3", "223", "Airbus", "A220-300", 130, 160},
    {"A221", "221", "Airbus", "A220-100", 108, 135},
    {"A223", "223", "Airbus", "A220-300", 130, 160},
    // Airbus A320 family
    {"A318", "318", "Airbus", "A318", 107, 107},
    {"A319", "319", "Airbus", "A319", 124, 156},
    {"A320", "320", "Airbus", "A320", 150, 186},
    {"A321", "321", "Airbus", "A321", 185, 236},
    {"A20N", "32N", "Airbus", "A320neo", 150, 194},
    {"A21N", "32Q", "Airbus", "A321neo", 185, 244},
    // Airbus A330/A340/A350/A380
    {"A332", "332", "Airbus", "A330-200", 210, 272},
    {"A333", "333", "Airbus", "A330-300", 277, 440},
    {"A338", "338", "Airbus", "A330-800", 220, 260},
    {"A339", "339", "Airbus", "A330-900", 260, 440},
    {"A343", "343", "Airbus", "A340-300", 295, 335},
    {"A346", "346", "Airbus", "A340-600", 326, 440},
    {"A359", "359", "Airbus", "A350-900", 300, 350},
    {"A35K", "351", "Airbus", "A350-1000", 350, 410},
    {"A388", "388", "Airbus", "A380-800", 555, 615},
    {"A306", "306", "Airbus", "A300-600", 250, 290},
    {"A310", "310", "Airbus", "A310-300", 220, 280},
    // Boeing 717/737/747/757/767/777/787 families
    {"B712", "717", "Boeing", "717-200", 110, 110},
    {"B736", "736", "Boeing", "737-600", 108, 132},
    {"B733", "733", "Boeing", "737-300", 126, 149},
    {"B734", "734", "Boeing", "737-400", 146, 170},
    {"B735", "735", "Boeing", "737-500", 108, 132},
    {"B737", "73G", "Boeing", "737-700", 126, 149},
    {"B738", "738", "Boeing", "737-800", 162, 189},
    {"B38M", "7M8", "Boeing", "737 MAX 8", 162, 210},
    {"B39M", "7M9", "Boeing", "737 MAX 9", 178, 220},
    {"B37M", "7M7", "Boeing", "737 MAX 7", 138, 172},
    // 737 MAX 10 ICAO varies by source; omit until confirmed
    {"B744", "744", "Boeing", "747-400", 416, 524},
    {"B748", "748", "Boeing", "747-8", 410, 524},
    {"B752", "752", "Boeing", "757-200", 200, 239},
    {"B753", "753", "Boeing", "757-300", 243, 280},
    {"B762", "762", "Boeing", "767-200", 181, 255},
    {"B763", "763", "Boeing", "767-300", 211, 269},
    {"B764", "764", "Boeing", "767-400", 245, 375},
    {"B772", "772", "Boeing", "777-200", 314, 396},
    {"B773", "773", "Boeing", "777-300", 368, 451},
    {"B77L", "77L", "Boeing", "777-200LR", 301, 317},
    {"B77W", "77W", "Boeing", "777-300ER", 365, 396},
    {"B788", "788", "Boeing", "787-8", 242, 290},
    {"B789", "789", "Boeing", "787-9", 280, 335},
    {"B78X", "78J", "Boeing", "787-10", 318, 440},
    {"B739", "739", "Boeing", "737-900", 178, 220},
    // Embraer E-Jets
    {"E170", "E70", "Embraer", "E170", 66, 78},
    {"E175", "E75", "Embraer", "E175", 76, 88},
    {"E75L", "E75", "Embraer", "E175 (E75L)", 76, 88},
    {"E190", "E90", "Embraer", "E190", 96, 114},
    {"E195", "E95", "Embraer", "E195", 108, 124},
    {"E290", "",   "Embraer", "E190-E2", 97, 120},
    {"E295", "",   "Embraer", "E195-E2", 120, 146},
    // Bombardier CRJ
    {"CRJ2", "CR2", "Bombardier", "CRJ-200", 50, 50},
    {"CRJ7", "CR7", "Bombardier", "CRJ-700", 66, 78},
    {"CRJ9", "CR9", "Bombardier", "CRJ-900", 76, 90},
    {"CRJX", "CRK", "Bombardier", "CRJ-1000", 90, 104},
    // De Havilland/Bombardier Dash 8
    {"DH8A", "",   "De Havilland Canada", "Dash 8-100", 37, 39},
    {"DH8B", "",   "De Havilland Canada", "Dash 8-200", 37, 39},
    {"DH8C", "",   "De Havilland Canada", "Dash 8-300", 50, 56},
    {"DH8D", "",   "De Havilland Canada", "Dash 8 Q400", 68, 90},
    // Business jets and GA (private)
    {"CL30", "",   "Bombardier", "Challenger 300", 8, 9},
    {"CL35", "",   "Bombardier", "Challenger 350", 8, 10},
    {"CL60", "",   "Bombardier", "Challenger 600", 10, 12},
    {"GLEX", "",   "Bombardier", "Global Express", 12, 19},
    {"GL7T", "",   "Bombardier", "Global 7500", 15, 19},
    {"GLF2", "",   "Gulfstream",  "Gulfstream II", 10, 14},
    {"GLF3", "",   "Gulfstream",  "Gulfstream III", 10, 14},
    {"GLF4", "",   "Gulfstream",  "Gulfstream IV", 12, 16},
    {"GLF5", "",   "Gulfstream",  "Gulfstream V", 14, 19},
    {"GLF6", "",   "Gulfstream",  "Gulfstream 650", 14, 19},
    {"C510", "",   "Cessna",      "Citation Mustang", 4, 5},
    {"C525", "",   "Cessna",      "CitationJet (CJ)", 5, 8},
    {"C25A", "",   "Cessna",      "Citation CJ1+", 5, 6},
    {"C25B", "",   "Cessna",      "Citation CJ3", 6, 8},
    {"C25C", "",   "Cessna",      "Citation CJ4", 7, 9},
    {"C56X", "",   "Cessna",      "Citation Excel/XLS", 8, 9},
    {"C680", "",   "Cessna",      "Citation Sovereign", 8, 12},
    {"C68A", "",   "Cessna",      "Citation Latitude", 8, 9},
    {"C700", "",   "Cessna",      "Citation Longitude", 8, 12},
    {"H25B", "",   "Hawker",      "Hawker 800", 8, 9},
    {"H25C", "",   "Hawker",      "Hawker 1000", 8, 9},
    {"LJ35", "",   "Learjet",     "Learjet 35", 6, 8},
    {"LJ40", "",   "Learjet",     "Learjet 40", 6, 7},
    {"LJ45", "",   "Learjet",     "Learjet 45", 8, 9},
    {"LJ60", "",   "Learjet",     "Learjet 60", 7, 8},
    {"LJ75", "",   "Learjet",     "Learjet 75", 8, 9},
    {"E50P", "",   "Embraer",     "Phenom 100", 4, 6},
    {"E55P", "",   "Embraer",     "Phenom 300", 6, 9},
    {"EA50", "",   "Eclipse",     "Eclipse 500", 4, 5},
    {"SF50", "",   "Cirrus",      "Vision Jet SF50", 5, 7},
    {"P180", "",   "Piaggio",     "P.180 Avanti", 7, 9},
    {"BE20", "",   "Beechcraft",  "King Air 200", 7, 9},
    {"BE58", "",   "Beechcraft",  "Baron 58", 6, 6},
    {"BE40", "",   "Beechcraft",  "Beechjet 400", 7, 8},
    {"PC24", "",   "Pilatus",     "PC-24", 6, 10},
    {"TBM7", "",   "Daher",        "TBM 700", 5, 6},
    {"TBM8", "",   "Daher",        "TBM 850", 5, 6},
    {"TBM9", "",   "Daher",        "TBM 900", 5, 6},
    {"P46T", "",   "Piper",        "PA-46 Meridian/M600", 5, 6},
    {"PA46", "",   "Piper",        "PA-46 Malibu/Mirage", 5, 6},
    {"PA28", "",   "Piper",        "PA-28 Cherokee", 2, 4},
    {"PA32", "",   "Piper",        "PA-32 Saratoga", 6, 6},
    {"PA44", "",   "Piper",        "PA-44 Seminole", 4, 4},
    {"PA18", "",   "Piper",        "PA-18 Super Cub", 2, 2},
    {"DA40", "",   "Diamond",      "DA40 Star", 4, 4},
    {"DA42", "",   "Diamond",      "DA42 Twin Star", 4, 4},
    {"DA62", "",   "Diamond",      "DA62", 7, 7},
    {"DA20", "",   "Diamond",      "DA20 Katana", 2, 2},
    {"M20P", "",   "Mooney",       "M20", 4, 4},
    {"F2TH", "",   "Dassault",    "Falcon 2000", 10, 12},
    {"FA50", "",   "Dassault",    "Falcon 50", 8, 9},
    {"FA7X", "",   "Dassault",    "Falcon 7X", 12, 16},
    {"FA8X", "",   "Dassault",    "Falcon 8X", 12, 16},
    {"F900", "",   "Dassault",    "Falcon 900", 12, 19},
    {"SR20", "",   "Cirrus",      "SR20", 4, 4},
    {"AT45", "ATR", "ATR", "ATR 42-500", 42, 50},
    {"AT43", "ATR", "ATR", "ATR 42-300", 42, 50},
    {"AT46", "ATR", "ATR", "ATR 42-600", 48, 50},
    {"AT72", "AT7", "ATR", "ATR 72", 68, 74},
    {"AT76", "AT7", "ATR", "ATR 72-600", 68, 78},
    {"AT75", "AT7", "ATR", "ATR 72-500", 68, 74},
    {"SF34", "",    "Saab",        "Saab 340", 33, 36},
    {"SB20", "",    "Saab",        "Saab 2000", 50, 58},
    {"F50",  "",    "Fokker",      "Fokker 50", 46, 58},
    {"F70",  "",    "Fokker",      "Fokker 70", 70, 80},
    {"F100", "",    "Fokker",      "Fokker 100", 97, 109},
    {"MU2",  "",    "Mitsubishi",  "MU-2", 6, 10},
    {"M28",  "",    "PZL",         "M28 Skytruck", 19, 19},
    {"C212", "",    "CASA",        "C-212 Aviocar", 21, 26},
    {"CN35", "",    "Airbus Military", "CN-235", 35, 45},
    {"C295", "",    "Airbus Military", "C-295", 48, 71},
    {"B461", "",    "BAe",         "BAe 146-100", 70, 82},
    {"B462", "",    "BAe",         "BAe 146-200", 85, 100},
    {"B463", "",    "BAe",         "BAe 146-300", 100, 128},
    {"RJ85", "",    "Avro",        "RJ85", 82, 112},
    {"RJ1H", "",    "Avro",        "RJ100", 97, 112},
    {"JS31", "",    "British Aerospace", "Jetstream 31", 18, 19},
    {"JS32", "",    "British Aerospace", "Jetstream 32", 19, 19},
    {"JS41", "",    "British Aerospace", "Jetstream 41", 29, 30},
    {"D328", "",    "Dornier",     "Do 328", 30, 33},
    {"J328", "",    "Dornier",     "328JET", 30, 33},
    {"D228", "",    "Dornier",     "Do 228", 18, 19},
    {"BN2P", "",    "Britten-Norman", "BN-2 Islander", 9, 10},
    {"L410", "",    "LET",         "L-410 Turbolet", 15, 19},
    {"E120", "",    "Embraer",     "EMB 120 Brasilia", 30, 30},
    {"E135", "",    "Embraer",     "ERJ 135", 37, 37},
    {"E145", "",    "Embraer",     "ERJ 145", 45, 50},
    {"C172", "", "Cessna", "172 Skyhawk", 4, 4},
    {"C182", "", "Cessna", "182 Skylane", 4, 4},
    {"T182", "", "Cessna", "T182 Turbo Skylane", 4, 4},
    {"C150", "", "Cessna", "150", 2, 2},
    {"C152", "", "Cessna", "152", 2, 2},
    {"C170", "", "Cessna", "170", 4, 4},
    {"C175", "", "Cessna", "175 Skylark", 4, 4},
    {"C177", "", "Cessna", "177 Cardinal", 4, 4},
    {"C206", "", "Cessna", "206 Stationair", 6, 6},
    {"T206", "", "Cessna", "T206 Turbo Stationair", 6, 6},
    {"U206", "", "Cessna", "U206 Stationair", 6, 6},
    {"P206", "", "Cessna", "P206 Pressurized Stationair", 6, 6},
    {"C207", "", "Cessna", "207 Stationair 7", 7, 7},
    {"C208", "", "Cessna", "208 Caravan", 9, 14},
    {"C208A", "", "Cessna", "208 Caravan Amphibian", 9, 14},
    {"C210", "", "Cessna", "210 Centurion", 6, 6},
    {"T210", "", "Cessna", "T210 Turbo Centurion", 6, 6},
    {"C310", "", "Cessna", "310", 4, 6},
    {"C185", "", "Cessna", "185 Skywagon", 4, 6},
    {"C180", "", "Cessna", "180 Skywagon", 4, 4},
    {"C350", "", "Cessna", "350 Corvalis", 4, 4},
    {"C400", "", "Cessna", "400 Corvalis TT", 4, 4},
    {"C414", "", "Cessna", "414 Chancellor", 6, 8},
    {"C421", "", "Cessna", "421 Golden Eagle", 6, 8},
    {"P28A", "", "Piper", "PA-28 Cherokee", 2, 4},
    {"P28R", "", "Piper", "PA-28R Arrow", 4, 4},
    {"P28T", "", "Piper", "PA-28RT Turbo Arrow", 4, 4},
    {"PA34", "", "Piper", "PA-34 Seneca", 6, 6},
    {"PA31", "", "Piper", "PA-31 Navajo/Chieftain", 6, 10},
    {"PA31T","", "Piper", "PA-31T Cheyenne", 6, 9},
    {"BE35", "", "Beechcraft", "Bonanza", 4, 6},
    {"BE33", "", "Beechcraft", "Debonair/Bonanza 33", 4, 6},
    {"BE36", "", "Beechcraft", "Bonanza A36", 6, 6},
    {"BE99", "", "Beechcraft", "Model 99 Airliner", 15, 17},
    {"B190", "", "Beechcraft", "1900", 19, 19},
    {"B350", "", "Beechcraft", "King Air 350", 8, 11},
    {"C402", "", "Cessna", "402", 6, 9},
    {"C441", "", "Cessna", "441 Conquest II", 8, 10},
    {"SR22", "", "Cirrus", "SR22", 4, 4},
    {"SR22T","", "Cirrus", "SR22T", 4, 4},
    {"GA8",  "", "GippsAero", "GA8 Airvan", 8, 8},
    {"PC6",  "", "Pilatus", "PC-6 Porter", 10, 10},
    {"DHC2", "", "de Havilland Canada", "DHC-2 Beaver", 6, 7},
    {"DHC3", "", "de Havilland Canada", "DHC-3 Otter", 10, 14},
    {"DH3T", "", "de Havilland Canada", "DHC-3T Turbo Otter", 10, 14},
    {"DHC3T", "", "de Havilland Canada", "DHC-3T Turbo Otter", 10, 14},
    {"DH6T", "", "de Havilland Canada", "DHC-6 Twin Otter", 19, 19},
    {"DHC7", "", "de Havilland Canada", "DHC-7 Dash 7", 40, 50},
    {"PC12", "", "Pilatus", "PC-12", 6, 9},
    {"KODI", "", "Quest", "Kodiak 100", 9, 10},
    {"DHC2T", "", "Viking Air", "DHC-2T Turbo Beaver", 6, 7},
    {"DH2T",  "", "Viking Air", "DHC-2T Turbo Beaver", 6, 7},
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
  // Family prefix fallbacks for unknown exact codes
  if (code.startsWith("A32")) { minOut = 150; maxOut = 244; return true; }
  if (code.startsWith("A33")) { minOut = 210; maxOut = 440; return true; }
  if (code.startsWith("A35")) { minOut = 300; maxOut = 410; return true; }
  if (code.startsWith("A22") || code.startsWith("BCS")) { minOut = 108; maxOut = 160; return true; }
  if (code.startsWith("B73")) { minOut = 126; maxOut = 220; return true; }
  if (code.startsWith("B74")) { minOut = 410; maxOut = 524; return true; }
  if (code.startsWith("B75")) { minOut = 200; maxOut = 280; return true; }
  if (code.startsWith("B76")) { minOut = 211; maxOut = 375; return true; }
  if (code.startsWith("B77")) { minOut = 314; maxOut = 451; return true; }
  if (code.startsWith("B78")) { minOut = 242; maxOut = 440; return true; }
  if (code.startsWith("E17")) { minOut = 66;  maxOut = 88;  return true; }
  if (code.startsWith("E19")) { minOut = 96;  maxOut = 124; return true; }
  if (code.startsWith("E29")) { minOut = 97;  maxOut = 146; return true; }
  if (code.startsWith("CRJ")) { minOut = 50;  maxOut = 104; return true; }
  if (code.startsWith("DH8")) { minOut = 39;  maxOut = 90;  return true; }
  if (code.startsWith("AT4")) { minOut = 42;  maxOut = 50;  return true; }
  if (code.startsWith("AT7")) { minOut = 68;  maxOut = 78;  return true; }
  if (code.startsWith("SF3")) { minOut = 33;  maxOut = 36;  return true; }
  if (code.startsWith("SB2")) { minOut = 50;  maxOut = 58;  return true; }
  if (code.startsWith("F50")) { minOut = 46;  maxOut = 58;  return true; }
  if (code.startsWith("F70")) { minOut = 70;  maxOut = 80;  return true; }
  if (code.startsWith("F10")) { minOut = 97;  maxOut = 109; return true; }
  if (code.startsWith("RJ8") || code.startsWith("RJ1")) { minOut = 82; maxOut = 112; return true; }
  if (code.startsWith("B46")) { minOut = 70;  maxOut = 128; return true; } // BAe 146 family
  if (code.startsWith("JS3")) { minOut = 18;  maxOut = 19;  return true; } // Jetstream 31/32
  if (code.startsWith("JS4")) { minOut = 29;  maxOut = 30;  return true; } // Jetstream 41
  if (code.startsWith("D32")) { minOut = 30;  maxOut = 33;  return true; } // Dornier 328/JET
  if (code.startsWith("D22")) { minOut = 18;  maxOut = 19;  return true; } // Dornier 228
  if (code.startsWith("BN2")) { minOut = 9;   maxOut = 10;  return true; } // Islander
  if (code.startsWith("L41")) { minOut = 15;  maxOut = 19;  return true; } // L-410
  if (code.startsWith("E12")) { minOut = 30;  maxOut = 30;  return true; } // EMB 120
  if (code.startsWith("E14")) { minOut = 37;  maxOut = 50;  return true; } // ERJ 135/145
  if (code.startsWith("MD8")) { minOut = 150; maxOut = 172; return true; }
  if (code.startsWith("MD9")) { minOut = 153; maxOut = 172; return true; }
  if (code.startsWith("GLF") || code.startsWith("GLEX") || code.startsWith("GL7T")) { minOut = 12; maxOut = 19; return true; }
  if (code.startsWith("CL3") || code.startsWith("CL6")) { minOut = 8; maxOut = 12; return true; }
  if (code.startsWith("LJ")) { minOut = 6; maxOut = 9; return true; }
  if (code.startsWith("C25") || code.startsWith("C68") || code.startsWith("C700") || code.startsWith("C56X")) { minOut = 7; maxOut = 12; return true; }
  if (code.startsWith("C51") || code.startsWith("EA50")) { minOut = 4; maxOut = 5; return true; }
  if (code.startsWith("BE2") || code.startsWith("BE3") || code.startsWith("BE4")) { minOut = 7; maxOut = 9; return true; }
  if (code.startsWith("PC12")) { minOut = 6; maxOut = 9; return true; }
  if (code.startsWith("TBM")) { minOut = 5; maxOut = 6; return true; }
  if (code.startsWith("PA46") || code.startsWith("P46T") || code.startsWith("PA34") || code.startsWith("PA32") || code.startsWith("PA31") || code.startsWith("PA44")) { minOut = 4; maxOut = 8; return true; }
  if (code.startsWith("DA4") || code.startsWith("DA6")) { minOut = 4; maxOut = 7; return true; }
  if (code.startsWith("SR2")) { minOut = 4; maxOut = 4; return true; }
  if (code.equals("SF50")) { minOut = 5; maxOut = 7; return true; }
  if (code.startsWith("PA28")) { minOut = 2; maxOut = 4; return true; }
  if (code.startsWith("C15")) { minOut = 2; maxOut = 2; return true; }
  if (code.startsWith("C31")) { minOut = 4; maxOut = 6; return true; }
  if (code.startsWith("C17")) { minOut = 4; maxOut = 4; return true; }
  if (code.startsWith("C18")) { minOut = 4; maxOut = 4; return true; }
  if (code.startsWith("C19")) { minOut = 4; maxOut = 6; return true; }
  if (code.startsWith("C20") || code.startsWith("T20") || code.startsWith("U20") || code.startsWith("P20")) { minOut = 6; maxOut = 7; return true; }
  if (code.startsWith("C40") || code.startsWith("C35")) { minOut = 4; maxOut = 4; return true; }
  if (code.startsWith("C41") || code.startsWith("C42")) { minOut = 6; maxOut = 8; return true; }
  if (code.startsWith("MU2")) { minOut = 6; maxOut = 10; return true; }
  if (code.startsWith("M28")) { minOut = 19; maxOut = 19; return true; }
  if (code.startsWith("C21") || code.startsWith("CN3") || code.startsWith("C29")) { minOut = 26; maxOut = 71; return true; }
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
