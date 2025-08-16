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
  { "BCS1", "221", "Airbus", "A220-100", 108, 135 },
  { "BCS3", "223", "Airbus", "A220-300", 130, 160 },
  { "A221", "221", "Airbus", "A220-100", 108, 135 },
  { "A223", "223", "Airbus", "A220-300", 130, 160 },
  { "A225", "225", "Airbus", "A220-500 (projected)", 150, 190 },
  // Airbus A320 family
  { "A318", "318", "Airbus", "A318", 107, 107 },
  { "A319", "319", "Airbus", "A319", 124, 156 },
  { "A320", "320", "Airbus", "A320", 150, 186 },
  { "A321", "321", "Airbus", "A321", 185, 236 },
  { "A20N", "32N", "Airbus", "A320neo", 150, 194 },
  { "A21N", "32Q", "Airbus", "A321neo", 185, 244 },
  // Airbus A330/A340
  { "A332", "332", "Airbus", "A330-200", 210, 260 },
  { "A333", "333", "Airbus", "A330-300", 250, 300 },
  { "A338", "338", "Airbus", "A330-800neo", 220, 260 },
  { "A339", "339", "Airbus", "A330-900neo", 260, 300 },
  { "A342", "342", "Airbus", "A340-200", 240, 261 },
  { "A343", "343", "Airbus", "A340-300", 250, 295 },
  { "A345", "345", "Airbus", "A340-500", 270, 310 },
  { "A346", "346", "Airbus", "A340-600", 310, 380 },
  // Airbus A350/A380
  { "A359", "359", "Airbus", "A350-900", 300, 350 },
  { "A35K", "35K", "Airbus", "A350-1000", 350, 410 },
  { "A35F", "",   "Airbus", "A350F (pax eq.)", 0, 0 },
  { "A388", "388", "Airbus", "A380-800", 450, 615 },
  // Airbus Regional/Older
  { "A300", "300", "Airbus", "A300", 240, 300 },
  { "A30B", "30B", "Airbus", "A300-600", 240, 300 },
  { "A306", "306", "Airbus", "A300-600R", 266, 304 },
  { "A300F", "",   "Airbus", "A300 Freighter (pax eq.)", 0, 0 },
  { "A310", "310", "Airbus", "A310", 190, 220 },
  { "A3ST", "",   "Airbus", "BelugaST", 5, 5 },

  // Boeing 737 family
  { "B731", "731", "Boeing", "737-100", 85, 104 },
  { "B732", "732", "Boeing", "737-200", 97, 130 },
  { "B733", "733", "Boeing", "737-300", 126, 149 },
  { "B734", "734", "Boeing", "737-400", 146, 168 },
  { "B735", "735", "Boeing", "737-500", 108, 132 },
  { "B736", "736", "Boeing", "737-600", 108, 132 },
  { "B737", "737", "Boeing", "737-700", 126, 149 },
  { "B738", "738", "Boeing", "737-800", 162, 189 },
  { "B739", "739", "Boeing", "737-900", 178, 220 },
  { "B38M", "7M8", "Boeing", "737 MAX 8", 162, 197 },
  { "B39M", "7M9", "Boeing", "737 MAX 9", 178, 220 },
  { "B37M", "7M7", "Boeing", "737 MAX 7", 138, 172 },
  { "B3XM", "7MX", "Boeing", "737 MAX 10", 204, 230 },
  // Boeing 747/757/767/777/787
  { "B701", "701", "Boeing", "707-120", 110, 179 },
  { "B703", "703", "Boeing", "707-320", 141, 189 },
  { "B720", "720", "Boeing", "720", 116, 149 },
  { "B721", "721", "Boeing", "727-100", 94, 131 },
  { "B722", "722", "Boeing", "727-200", 145, 189 },
  { "B741", "741", "Boeing", "747-100", 366, 452 },
  { "B742", "742", "Boeing", "747-200", 366, 452 },
  { "B743", "743", "Boeing", "747-300", 412, 496 },
  { "B744", "744", "Boeing", "747-400", 416, 524 },
  { "B741F", "",   "Boeing", "747-100F (pax eq.)", 0, 0 },
  { "BLCF", "",    "Boeing", "747 LCF Dreamlifter", 8, 8 },
  { "B748", "748", "Boeing", "747-8", 410, 467 },
  { "B752", "752", "Boeing", "757-200", 178, 235 },
  { "B753", "753", "Boeing", "757-300", 243, 295 },
  { "B762", "762", "Boeing", "767-200", 181, 255 },
  { "B763", "763", "Boeing", "767-300", 218, 269 },
  { "B764", "764", "Boeing", "767-400ER", 245, 304 },
  { "B772", "772", "Boeing", "777-200", 314, 396 },
  { "B77L", "77L", "Boeing", "777-200LR", 317, 317 },
  { "B773", "773", "Boeing", "777-300", 368, 451 },
  { "B77W", "77W", "Boeing", "777-300ER", 365, 451 },
  { "B778", "778", "Boeing", "777-8", 300, 384 },
  { "B779", "779", "Boeing", "777-9", 350, 426 },
  { "B788", "788", "Boeing", "787-8", 242, 248 },
  { "B789", "789", "Boeing", "787-9", 290, 296 },
  { "B78X", "78X", "Boeing", "787-10", 318, 330 },

  // Embraer E-Jets & E2
  { "E170", "E70", "Embraer", "E170", 66, 78 },
  { "E175", "E75", "Embraer", "E175", 76, 88 },
  { "E75L", "E75", "Embraer", "E175 (long wing)", 76, 88 },
  { "E75S", "E75", "Embraer", "E175 (short wing)", 70, 88 },
  { "E190", "E90", "Embraer", "E190", 97, 114 },
  { "E195", "E95", "Embraer", "E195", 108, 132 },
  { "E290", "", "Embraer", "E190-E2", 97, 120 },
  { "E295", "", "Embraer", "E195-E2", 120, 146 },

  // Canadair / Bombardier CRJ & Q400
  { "CRJ1", "CR1", "Bombardier", "CRJ100", 50, 50 },
  { "CRJ2", "CR2", "Bombardier", "CRJ200", 50, 50 },
  { "CRJ7", "CR7", "Bombardier", "CRJ700", 66, 78 },
  { "CRJ9", "CR9", "Bombardier", "CRJ900", 76, 90 },
  { "CRJX", "CRK", "Bombardier", "CRJ1000", 97, 104 },
  { "DH8D", "DH4", "De Havilland Canada", "Dash 8 Q400", 68, 90 },
  { "DH8A", "DH1", "De Havilland Canada", "Dash 8-100", 37, 39 },
  { "DH8B", "DH2", "De Havilland Canada", "Dash 8-200", 39, 40 },
  { "DH8C", "DH3", "De Havilland Canada", "Dash 8-300", 50, 56 },

  // ATR
  { "AT45", "AT5", "ATR", "ATR 42-500", 46, 50 },
  { "AT46", "AT6", "ATR", "ATR 42-600", 46, 50 },
  { "AT72", "AT7", "ATR", "ATR 72", 68, 78 },
  { "AT76", "ATR", "ATR", "ATR 72-600", 68, 78 },

  // Commuters / Turboprops
  { "SF34", "SF3", "Saab", "340B", 33, 36 },
  { "SB20", "S20", "Saab", "2000", 50, 58 },
  { "BE20", "", "Beechcraft", "Super King Air 200", 7, 13 },
  { "BE30", "", "Beechcraft", "Super King Air 300/350", 8, 11 },
  { "B190", "", "Beechcraft", "1900/1900D", 19, 19 },
  { "D228", "", "Dornier", "Do 228", 18, 19 },
  { "D328", "", "Dornier", "Do 328-100", 30, 33 },
  { "J328", "", "Fairchild Dornier", "328JET", 30, 33 },
  { "JS31", "", "British Aerospace", "Jetstream 31", 18, 19 },
  { "JS41", "", "British Aerospace", "Jetstream 41", 29, 30 },
  { "F50", "", "Fokker", "50", 46, 62 },
  { "F70", "F70", "Fokker", "70", 72, 85 },
  { "F100", "100", "Fokker", "100", 97, 109 },
  { "YS11", "", "NAMC", "YS-11", 60, 64 },
  { "BA11", "", "British Aerospace", "BAe 146-100", 70, 82 },
  { "BA12", "", "British Aerospace", "BAe 146-200", 85, 100 },
  { "BA13", "", "British Aerospace", "BAe 146-300", 100, 116 },

  // Regional jets
  { "ARJ1", "AR1", "Comac", "ARJ21-700", 78, 90 },
  { "SU95", "SU9", "Sukhoi", "Superjet 100", 87, 108 },
  { "MRJ9", "M90", "Mitsubishi", "SpaceJet M90", 88, 92 },
  { "C919", "",   "Comac", "C919", 158, 174 },
  { "E135", "",   "Embraer", "ERJ 135", 37, 37 },
  { "E140", "",   "Embraer", "ERJ 140", 44, 44 },
  { "E145", "",   "Embraer", "ERJ 145", 45, 50 },
  { "E45X", "",   "Embraer", "ERJ 145XR", 50, 50 },

  // Bizjets (common)
  { "C25A", "", "Cessna", "CJ2", 6, 8 },
  { "C25B", "", "Cessna", "CJ3", 6, 8 },
  { "C25C", "", "Cessna", "CJ4", 7, 9 },
  { "C510", "", "Cessna", "Citation Mustang", 4, 5 },
  { "C525", "", "Cessna", "CitationJet CJ1", 5, 6 },
  { "C550", "", "Cessna", "Citation II/Bravo", 7, 9 },
  { "C560", "", "Cessna", "Citation V/Ultra/Encore", 7, 9 },
  { "C56X", "", "Cessna", "Citation Excel/XLS", 8, 9 },
  { "E50P", "", "Embraer", "Phenom 100", 4, 6 },
  { "E55P", "", "Embraer", "Phenom 300", 6, 9 },
  { "FA50", "", "Dassault", "Falcon 50", 9, 9 },
  { "F2TH", "", "Dassault", "Falcon 2000", 8, 12 },
  { "FA7X", "", "Dassault", "Falcon 7X", 12, 16 },
  { "GLF2", "", "Gulfstream", "GII", 14, 19 },
  { "GLF3", "", "Gulfstream", "GIII", 14, 19 },
  { "GLF4", "", "Gulfstream", "GIV", 14, 19 },
  { "GLF5", "", "Gulfstream", "GV", 14, 19 },
  { "GLF6", "", "Gulfstream", "G650/G650ER", 14, 19 },
  { "GL7T", "", "Bombardier", "Global 7500", 14, 19 },
  { "CL30", "C30", "Bombardier", "Challenger 300", 8, 9 },
  { "CL35", "C35", "Bombardier", "Challenger 350", 8, 10 },
  { "CL60", "CRJ", "Bombardier", "Challenger 600", 9, 12 },
  { "LR35", "", "Learjet", "35", 6, 8 },
  { "LR45", "", "Learjet", "45", 8, 8 },
  { "LR60", "", "Learjet", "60", 7, 8 },
  { "PC24", "", "Pilatus", "PC-24", 6, 10 },
  { "HDJT", "", "Honda Aircraft", "HondaJet HA-420", 4, 5 },
  { "F2TP", "", "Dassault", "Falcon 2000S/LXS", 8, 12 },

  // GA singles and twins (a sampling for sanity)
  { "C150", "", "Cessna", "150", 2, 2 },
  { "C152", "", "Cessna", "152", 2, 2 },
  { "C172", "", "Cessna", "172", 4, 4 },
  { "C182", "", "Cessna", "182", 4, 4 },
  { "C206", "", "Cessna", "206", 6, 6 },
  { "T206", "", "Cessna", "T206 Turbo Stationair", 6, 6 },
  { "U206", "", "Cessna", "U206 Stationair", 6, 6 },
  { "P206", "", "Cessna", "P206 Pressurized Stationair", 6, 6 },
  { "C207", "", "Cessna", "207 Stationair 7", 7, 7 },
  { "C208", "", "Cessna", "208 Caravan", 9, 12 },
  { "C208A", "", "Cessna", "208 Caravan Amphibian", 9, 12 },
  { "C210", "", "Cessna", "210", 6, 6 },
  { "C337", "", "Cessna", "337 Skymaster", 4, 6 },
  { "C350", "", "Cessna", "350 Corvalis", 4, 4 },
  { "C400", "", "Cessna", "400 Corvalis TT", 4, 4 },
  { "BE58", "", "Beechcraft", "Baron 58", 6, 6 },
  { "BE99", "", "Beechcraft", "Model 99 Airliner", 15, 17 },
  { "PA31", "", "Piper", "Navajo/Chieftain", 6, 9 },
  { "PA34", "", "Piper", "Seneca", 6, 6 },
  { "PA44", "", "Piper", "Seminole", 4, 4 },
  { "PA46", "", "Piper", "Malibu/Mirage/Meridian", 5, 6 },
  { "M600", "", "Piper", "M600", 5, 6 },
  { "P28A", "", "Piper", "PA-28 Archer", 4, 4 },
  { "P28R", "", "Piper", "PA-28R Arrow", 4, 4 },
  { "SR20", "", "Cirrus", "SR20", 4, 4 },
  { "SR22", "", "Cirrus", "SR22", 4, 5 },
  { "SF50", "", "Cirrus", "Vision Jet SF50", 5, 7 },
  { "DA40", "", "Diamond", "DA40", 4, 4 },
  { "DA42", "", "Diamond", "DA42 Twin Star", 4, 4 },
  { "DA62", "", "Diamond", "DA62", 7, 7 },
  { "M20P", "", "Mooney", "M20J", 4, 4 },
  { "PC12", "", "Pilatus", "PC-12", 6, 9 },
  { "KODI", "", "Quest", "Kodiak 100", 9, 10 },
  { "EA50", "", "Eclipse", "Eclipse 500", 4, 5 },
  { "TBM7", "", "Daher", "TBM 700", 5, 6 },
  { "TBM8", "", "Daher", "TBM 850", 5, 6 },
  { "TBM9", "", "Daher", "TBM 900", 5, 6 },

  // Helicopters (common)
  { "R22", "", "Robinson", "R22", 2, 2 },
  { "R44", "", "Robinson", "R44", 4, 4 },
  { "R66", "", "Robinson", "R66", 4, 5 },
  { "B06", "", "Bell", "206", 4, 6 },
  { "B407", "", "Bell", "407", 5, 6 },
  { "B412", "", "Bell", "412", 13, 15 },
  { "UH60", "", "Sikorsky", "UH-60 Black Hawk", 11, 14 },
  { "CH47", "", "Boeing", "CH-47 Chinook", 33, 55 },
  { "EC35", "", "Airbus Helicopters", "H135/EC135", 6, 7 },
  { "EC55", "", "Airbus Helicopters", "H155/EC155", 10, 13 },
  { "H160", "", "Airbus Helicopters", "H160", 10, 12 },
  { "A139", "", "AgustaWestland", "AW139", 12, 15 },
  { "A169", "", "AgustaWestland", "AW169", 8, 10 },
  { "A189", "", "AgustaWestland", "AW189", 14, 19 },
  { "S76", "", "Sikorsky", "S-76", 12, 13 },
  { "S92", "", "Sikorsky", "S-92", 19, 19 },
  { "MI8",  "", "Mil", "Mi-8/17 Hip", 24, 36 },
  { "KA32", "", "Kamov", "Ka-32", 12, 16 },

  // Seaplanes and bush
  { "DHC2", "", "de Havilland Canada", "DHC-2 Beaver", 6, 7 },
  { "DHC3", "", "de Havilland Canada", "DHC-3 Otter", 10, 11 },
  { "DHC6", "", "de Havilland Canada", "DHC-6 Twin Otter", 19, 19 },
  { "LA8", "", "Lake Aircraft", "LA-8", 4, 6 },
  { "AN2", "", "Antonov", "An-2", 12, 12 },

  // Military (subset; for detection and seat caps where relevant)
  { "F16", "", "General Dynamics", "F-16 Fighting Falcon", 1, 2 },
  { "F18", "", "McDonnell Douglas/Boeing", "F/A-18 Hornet", 1, 2 },
  { "F22", "", "Lockheed Martin", "F-22 Raptor", 1, 1 },
  { "F35", "", "Lockheed Martin", "F-35 Lightning II", 1, 2 },
  { "B2", "", "Northrop Grumman", "B-2 Spirit", 2, 2 },
  { "B52", "", "Boeing", "B-52 Stratofortress", 5, 8 },
  { "C17", "", "Boeing", "C-17 Globemaster III", 3, 170 },
  { "C5",  "", "Lockheed Martin", "C-5 Galaxy", 15, 345 },
  { "C130", "", "Lockheed Martin", "C-130 Hercules", 2, 92 },
  { "KC10", "", "McDonnell Douglas", "KC-10 Extender", 75, 75 },
  { "KC46", "", "Boeing", "KC-46 Pegasus", 65, 65 },
  { "P8",   "", "Boeing", "P-8 Poseidon", 9, 11 },
  { "E3TF", "", "Boeing", "E-3 Sentry AWACS", 13, 19 },
  { "E7",   "", "Boeing", "E-7 Wedgetail", 12, 12 },
  { "C27J", "", "Leonardo", "C-27J Spartan", 2, 60 },
  { "CN35", "", "Airbus Military", "CN-235", 35, 45 },
  { "C295", "", "Airbus Military", "C-295", 48, 71 },
  { "TU154", "T54", "Tupolev", "Tu-154", 164, 180 },
  { "IL96",  "I96", "Ilyushin", "Il-96", 262, 300 },
  { "AN148", "",   "Antonov", "An-148", 68, 85 },
  { "AN158", "",   "Antonov", "An-158", 97, 99 },
  { "EUFI", "", "Eurofighter", "Typhoon", 1, 2 },

  // --- Additions (curated for broader coverage; memory-conscious) ---
  { "HDJT", "", "Honda Aircraft", "HondaJet HA-420", 4, 5 },
  { "E55P", "", "Embraer", "Phenom 300", 6, 9 },
  { "F2TP", "", "Dassault", "Falcon 2000S/LXS", 8, 12 },
  { "EC35", "", "Airbus Helicopters", "H135/EC135", 6, 7 },
  { "A139", "", "AgustaWestland", "AW139", 12, 15 },
  { "A169", "", "AgustaWestland", "AW169", 8, 10 },
  { "A189", "", "AgustaWestland", "AW189", 14, 19 },
  { "EC55", "", "Airbus Helicopters", "H155/EC155", 10, 13 },
  { "H160", "", "Airbus Helicopters", "H160", 10, 12 },
  { "B190", "", "Beechcraft", "1900/1900D", 19, 19 },
  { "JS31", "", "British Aerospace", "Jetstream 31", 18, 19 },
  { "JS41", "", "British Aerospace", "Jetstream 41", 29, 30 },
  { "D228", "", "Dornier", "Do 228", 18, 19 },
  { "D328", "", "Dornier", "Do 328-100", 30, 33 },
  { "J328", "", "Fairchild Dornier", "328JET", 30, 33 },
  { "C208A", "", "Cessna", "208 Caravan Amphibian", 9, 12 },
  { "DHC6", "", "de Havilland Canada", "DHC-6 Twin Otter", 19, 19 },
  { "LA8", "", "Lake Aircraft", "LA-8", 4, 6 },
  { "E290", "", "Embraer", "E190-E2", 97, 120 },
  { "E295", "", "Embraer", "E195-E2", 120, 146 },
};

// Lightweight table: ICAO → preferred display name (used when memory tight)
struct AircraftTypeName {
  const char* icao;
  const char* name;
};

static const AircraftTypeName kAircraftTypes[] = {
  { "A318", "Airbus A318" },
  { "A319", "Airbus A319" },
  { "A320", "Airbus A320" },
  { "A321", "Airbus A321" },
  { "A20N", "Airbus A320neo" },
  { "A21N", "Airbus A321neo" },
  { "A332", "Airbus A330-200" },
  { "A333", "Airbus A330-300" },
  { "A338", "Airbus A330-800neo" },
  { "A339", "Airbus A330-900neo" },
  { "A342", "Airbus A340-200" },
  { "A343", "Airbus A340-300" },
  { "A345", "Airbus A340-500" },
  { "A346", "Airbus A340-600" },
  { "A359", "Airbus A350-900" },
  { "A35K", "Airbus A350-1000" },
  { "A388", "Airbus A380-800" },
  { "B731", "Boeing 737-100" },
  { "B732", "Boeing 737-200" },
  { "B733", "Boeing 737-300" },
  { "B734", "Boeing 737-400" },
  { "B735", "Boeing 737-500" },
  { "B736", "Boeing 737-600" },
  { "B737", "Boeing 737-700" },
  { "B738", "Boeing 737-800" },
  { "B739", "Boeing 737-900" },
  { "B37M", "Boeing 737 MAX 7" },
  { "B38M", "Boeing 737 MAX 8" },
  { "B39M", "Boeing 737 MAX 9" },
  { "B3XM", "Boeing 737 MAX 10" },
  { "B741", "Boeing 747-100" },
  { "B742", "Boeing 747-200" },
  { "B743", "Boeing 747-300" },
  { "B744", "Boeing 747-400" },
  { "B748", "Boeing 747-8" },
  { "B752", "Boeing 757-200" },
  { "B753", "Boeing 757-300" },
  { "B762", "Boeing 767-200" },
  { "B763", "Boeing 767-300" },
  { "B764", "Boeing 767-400ER" },
  { "B772", "Boeing 777-200" },
  { "B77L", "Boeing 777-200LR" },
  { "B773", "Boeing 777-300" },
  { "B77W", "Boeing 777-300ER" },
  { "B778", "Boeing 777-8" },
  { "B779", "Boeing 777-9" },
  { "B788", "Boeing 787-8" },
  { "B789", "Boeing 787-9" },
  { "B78X", "Boeing 787-10" },
  { "BCS1", "Airbus A220-100" },
  { "BCS3", "Airbus A220-300" },
  { "A221", "Airbus A220-100" },
  { "A223", "Airbus A220-300" },
  { "E170", "Embraer E170" },
  { "E175", "Embraer E175" },
  { "E75L", "Embraer E175" },
  { "E75S", "Embraer E175" },
  { "E190", "Embraer E190" },
  { "E195", "Embraer E195" },
  { "E290", "Embraer E190-E2" },
  { "E295", "Embraer E195-E2" },
  { "E135", "Embraer ERJ 135" },
  { "E140", "Embraer ERJ 140" },
  { "E145", "Embraer ERJ 145" },
  { "E45X", "Embraer ERJ 145XR" },
  { "CRJ1", "Bombardier CRJ100" },
  { "CRJ2", "Bombardier CRJ200" },
  { "CRJ7", "Bombardier CRJ700" },
  { "CRJ9", "Bombardier CRJ900" },
  { "CRJX", "Bombardier CRJ1000" },
  { "DH8A", "DHC-8-100" },
  { "DH8B", "DHC-8-200" },
  { "DH8C", "DHC-8-300" },
  { "DH8D", "DHC-8-400 (Q400)" },
  { "AT45", "ATR 42-500" },
  { "AT46", "ATR 42-600" },
  { "AT72", "ATR 72" },
  { "AT76", "ATR 72-600" },
  { "SF34", "Saab 340B" },
  { "SB20", "Saab 2000" },
  { "BE20", "Beech Super King Air 200" },
  { "BE30", "Beech Super King Air 300/350" },
  { "B190", "Beechcraft 1900D" },
  { "D228", "Dornier Do 228" },
  { "D328", "Dornier 328-100" },
  { "J328", "Fairchild Dornier 328JET" },
  { "JS31", "BAe Jetstream 31" },
  { "JS41", "BAe Jetstream 41" },
  { "F50", "Fokker 50" },
  { "ARJ1", "Comac ARJ21-700" },
  { "SU95", "Sukhoi Superjet 100" },
  { "MRJ9", "Mitsubishi SpaceJet M90" },
  { "C25A", "Cessna Citation CJ2" },
  { "C25B", "Cessna Citation CJ3" },
  { "C25C", "Cessna Citation CJ4" },
  { "C510", "Cessna Citation Mustang" },
  { "C525", "Cessna CitationJet CJ1" },
  { "C550", "Cessna Citation II/Bravo" },
  { "C560", "Cessna Citation V/Ultra/Encore" },
  { "C56X", "Cessna Citation Excel/XLS" },
  { "E50P", "Embraer Phenom 100" },
  { "E55P", "Embraer Phenom 300" },
  { "FA50", "Dassault Falcon 50" },
  { "F2TH", "Dassault Falcon 2000" },
  { "FA7X", "Dassault Falcon 7X" },
  { "GLF2", "Gulfstream II" },
  { "GLF3", "Gulfstream III" },
  { "GLF4", "Gulfstream IV" },
  { "GLF5", "Gulfstream V" },
  { "GLF6", "Gulfstream G650" },
  { "GL7T", "Bombardier Global 7500" },
  { "CL30", "Challenger 300" },
  { "CL35", "Challenger 350" },
  { "CL60", "Challenger 600" },
  { "LR35", "Learjet 35" },
  { "LR45", "Learjet 45" },
  { "LR60", "Learjet 60" },
  { "PC24", "Pilatus PC-24" },
  { "HDJT", "HondaJet HA-420" },
  { "F2TP", "Falcon 2000S/LXS" },
  { "C150", "Cessna 150" },
  { "C152", "Cessna 152" },
  { "C172", "Cessna 172" },
  { "C182", "Cessna 182" },
  { "C206", "Cessna 206" },
  { "T206", "Cessna T206 Turbo Stationair" },
  { "U206", "Cessna U206 Stationair" },
  { "P206", "Cessna P206 Pressurized Stationair" },
  { "C207", "Cessna 207 Stationair 7" },
  { "C208", "Cessna 208 Caravan" },
  { "C208A", "Cessna 208 Caravan Amphibian" },
  { "C210", "Cessna 210" },
  { "C350", "Cessna 350 Corvalis" },
  { "C400", "Cessna 400 Corvalis TT" },
  { "BE58", "Beechcraft Baron 58" },
  { "BE99", "Beechcraft Model 99 Airliner" },
  { "PA31", "Piper Navajo/Chieftain" },
  { "PA34", "Piper Seneca" },
  { "PA44", "Piper Seminole" },
  { "PA46", "Piper Malibu/Mirage/Meridian" },
  { "P28A", "Piper PA-28 Archer" },
  { "P28R", "Piper PA-28R Arrow" },
  { "SR20", "Cirrus SR20" },
  { "SR22", "Cirrus SR22" },
  { "SF50", "Cirrus Vision Jet SF50" },
  { "DA40", "Diamond DA40" },
  { "DA42", "Diamond DA42" },
  { "DA62", "Diamond DA62" },
  { "M20P", "Mooney M20" },
  { "PC12", "Pilatus PC-12" },
  { "KODI", "Quest Kodiak 100" },
  { "EA50", "Eclipse 500" },
  { "TBM7", "Daher TBM 700" },
  { "TBM8", "Daher TBM 850" },
  { "TBM9", "Daher TBM 900" },
  { "R22", "Robinson R22" },
  { "R44", "Robinson R44" },
  { "R66", "Robinson R66" },
  { "B06", "Bell 206" },
  { "B407", "Bell 407" },
  { "B412", "Bell 412" },
  { "EC35", "Airbus H135/EC135" },
  { "EC55", "Airbus H155/EC155" },
  { "H160", "Airbus H160" },
  { "A139", "AgustaWestland AW139" },
  { "A169", "AgustaWestland AW169" },
  { "A189", "AgustaWestland AW189" },
  { "S76", "Sikorsky S-76" },
  { "S92", "Sikorsky S-92" },
  { "DHC2", "DHC-2 Beaver" },
  { "DHC3", "DHC-3 Otter" },
  { "DHC6", "DHC-6 Twin Otter" },
  { "LA8", "Lake LA-8" },
  { "AN2", "Antonov An-2" },
  { "F16", "F-16 Fighting Falcon" },
  { "F18", "F/A-18 Hornet" },
  { "F22", "F-22 Raptor" },
  { "F35", "F-35 Lightning II" },
  { "B2", "B-2 Spirit" },
  { "B52", "B-52 Stratofortress" },
  { "C17", "C-17 Globemaster III" },
  { "C130", "C-130 Hercules" },
  { "EUFI", "Eurofighter Typhoon" },
};

// --- Lookup helpers ---

// Find the preferred display name using the lightweight table first (fast & tiny),
// and fall back to the richer info table if needed.
inline const char* aircraftDisplayName(const char* icao) {
  if (!icao || !*icao) return "Unknown";
  for (size_t i = 0; i < sizeof(kAircraftTypes) / sizeof(kAircraftTypes[0]); ++i) {
    if (strcasecmp(icao, kAircraftTypes[i].icao) == 0) {
      return kAircraftTypes[i].name;
    }
  }
  for (size_t i = 0; i < sizeof(kTypeInfo) / sizeof(kTypeInfo[0]); ++i) {
    if (strcasecmp(icao, kTypeInfo[i].icao) == 0) {
      return kTypeInfo[i].model;
    }
  }
  return "Unknown";
}

// Return a conservative seat range (min,max). If unknown, return (1, 6) for GA-ish defaults.
// You can tune the default if your app’s domain skews commercial vs GA.
inline void aircraftSeatRange(const char* icao, uint16_t& outMin, uint16_t& outMax) {
  outMin = 1;
  outMax = 6;  // sensible GA default
  if (!icao || !*icao) return;

  // Direct match against rich table first.
  for (size_t i = 0; i < sizeof(kTypeInfo) / sizeof(kTypeInfo[0]); ++i) {
    if (strcasecmp(icao, kTypeInfo[i].icao) == 0) {
      outMin = kTypeInfo[i].minSeats;
      outMax = kTypeInfo[i].maxSeats;
      return;
    }
  }

  // Family heuristics (keep short & cheap):
  // A32X
  if (strncasecmp(icao, "A31", 3) == 0 || strncasecmp(icao, "A32", 3) == 0) {
    outMin = 150;
    outMax = 244;
    return;
  }
  // B70X (707)
  if (strncasecmp(icao, "B70", 3) == 0) {
    outMin = 110;
    outMax = 189;
    return;
  }
  // B72X (727)
  if (strncasecmp(icao, "B72", 3) == 0) {
    outMin = 94;
    outMax = 189;
    return;
  }
  // B73X
  if (strncasecmp(icao, "B73", 3) == 0) {
    outMin = 108;
    outMax = 230;
    return;
  }
  // B78X
  if (strncasecmp(icao, "B78", 3) == 0) {
    outMin = 242;
    outMax = 330;
    return;
  }
  // E-Jets
  if (strncasecmp(icao, "E17", 3) == 0 || strncasecmp(icao, "E19", 3) == 0 || strncasecmp(icao, "E29", 3) == 0 || strncasecmp(icao, "E75", 3) == 0) {
    outMin = 66;
    outMax = 146;
    return;
  }
  // CRJ
  if (strncasecmp(icao, "CRJ", 3) == 0) {
    outMin = 50;
    outMax = 104;
    return;
  }
  // ATR
  if (strncasecmp(icao, "AT4", 3) == 0 || strncasecmp(icao, "AT7", 3) == 0) {
    outMin = 46;
    outMax = 78;
    return;
  }
  // Dash 8
  if (strncasecmp(icao, "DH8", 3) == 0) {
    outMin = 37;
    outMax = 90;
    return;
  }
  // BAe 146 family (BA11/12/13)
  if (strncasecmp(icao, "BA1", 3) == 0) {
    outMin = 70;
    outMax = 116;
    return;
  }
  // YS-11
  if (strncasecmp(icao, "YS1", 3) == 0) {
    outMin = 60;
    outMax = 64;
    return;
  }
  // C919
  if (strncasecmp(icao, "C91", 3) == 0) {
    outMin = 158;
    outMax = 174;
    return;
  }
  // GA Caravan
  if (strcasecmp(icao, "C208") == 0 || strcasecmp(icao, "C208A") == 0) {
    outMin = 9;
    outMax = 12;
    return;
  }
  // Helicopters (common)
  if (strcasecmp(icao, "R44") == 0) {
    outMin = 4;
    outMax = 4;
    return;
  }
  if (strcasecmp(icao, "A139") == 0) {
    outMin = 12;
    outMax = 15;
    return;
  }
}

// --- Arduino String shims (backward-compatible with existing .ino code) ---
// Friendly name from String code (prefers lightweight mapping; falls back to rich model)
inline String aircraftFriendlyName(const String& rawCode) {
  if (rawCode.length() == 0) return String("");
  String code = rawCode; code.trim(); code.toUpperCase();
  const char* name = aircraftDisplayName(code.c_str());
  if (!name || strcmp(name, "Unknown") == 0) return String("");
  return String(name);
}

// Seat range from String code; returns true if a specific or heuristic match applied.
inline bool aircraftSeatRange(const String& rawCode, uint16_t& minOut, uint16_t& maxOut) {
  if (rawCode.length() == 0) return false;
  String code = rawCode; code.trim(); code.toUpperCase();
  // Exact in rich table
  for (size_t i = 0; i < sizeof(kTypeInfo)/sizeof(kTypeInfo[0]); ++i) {
    if (code.equalsIgnoreCase(kTypeInfo[i].icao) || (kTypeInfo[i].iata && strlen(kTypeInfo[i].iata) && code.equalsIgnoreCase(kTypeInfo[i].iata))) {
      minOut = kTypeInfo[i].minSeats; maxOut = kTypeInfo[i].maxSeats; return true;
    }
  }
  // Heuristics mirroring the C-string helper
  auto pref = [&](const char* pfx){ return strncasecmp(code.c_str(), pfx, strlen(pfx)) == 0; };
  auto eq   = [&](const char* s){ return strcasecmp(code.c_str(), s) == 0; };
  if (pref("A31") || pref("A32")) { minOut=150; maxOut=244; return true; }
  if (pref("B70")) { minOut=110; maxOut=189; return true; }
  if (pref("B72")) { minOut=94;  maxOut=189; return true; }
  if (pref("B73")) { minOut=108; maxOut=230; return true; }
  if (pref("B78")) { minOut=242; maxOut=330; return true; }
  if (pref("E17") || pref("E19") || pref("E29") || pref("E75")) { minOut=66; maxOut=146; return true; }
  if (pref("CRJ")) { minOut=50; maxOut=104; return true; }
  if (pref("AT4") || pref("AT7")) { minOut=46; maxOut=78; return true; }
  if (pref("DH8")) { minOut=37; maxOut=90; return true; }
  if (pref("BA1")) { minOut=70; maxOut=116; return true; }
  if (pref("YS1")) { minOut=60; maxOut=64; return true; }
  if (pref("C91")) { minOut=158; maxOut=174; return true; }
  if (eq("C208") || eq("C208A")) { minOut=9; maxOut=12; return true; }
  if (eq("R44")) { minOut=4; maxOut=4; return true; }
  if (eq("A139")) { minOut=12; maxOut=15; return true; }
  // GA broad defaults for families commonly encountered
  if (pref("PA28")) { minOut=2; maxOut=4; return true; }
  if (pref("PA31")) { minOut=6; maxOut=10; return true; }
  if (pref("PA34")) { minOut=6; maxOut=6; return true; }
  if (pref("PA44")) { minOut=4; maxOut=4; return true; }
  if (pref("PA46") || pref("P46T")) { minOut=5; maxOut=6; return true; }
  if (pref("C15")) { minOut=2; maxOut=2; return true; }
  if (pref("C17") || pref("C18")) { minOut=4; maxOut=4; return true; }
  if (pref("C19")) { minOut=4; maxOut=6; return true; }
  if (pref("C20") || pref("T20") || pref("U20") || pref("P20")) { minOut=6; maxOut=7; return true; }
  if (pref("C40") || pref("C35")) { minOut=4; maxOut=4; return true; }
  if (pref("C41") || pref("C42")) { minOut=6; maxOut=8; return true; }
  if (pref("MU2")) { minOut=6; maxOut=10; return true; }
  if (pref("M28")) { minOut=19; maxOut=19; return true; }
  if (pref("C21") || pref("CN3") || pref("C29")) { minOut=26; maxOut=71; return true; }
  if (eq("SF50")) { minOut=5; maxOut=7; return true; }
  return false;
}

// Compose display string "CODE FriendlyName" with graceful fallbacks
inline String aircraftDisplayType(const String& rawCode) {
  String code = rawCode; code.trim();
  String name = aircraftFriendlyName(code);
  if (!name.length()) return code;        // fallback to code
  if (!code.length()) return name;        // safety
  return code + " " + name;
}
