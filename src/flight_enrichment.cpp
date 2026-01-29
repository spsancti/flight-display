#include "flight_enrichment.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

#include "aircraft_types.h"
#include "app_config.h"
#include "config_features.h"
#include "log.h"

struct MilCacheEntry {
  String hex;
  uint32_t ts = 0;
  bool isMil = false;
};
static const size_t kMilCacheSize = 16;
static MilCacheEntry g_milCache[kMilCacheSize];

struct HexDbCacheEntry {
  String hex;
  String name;
  String icaoType;
  String owner;
  uint32_t ts = 0;
};
#ifndef HEXDB_CACHE_TTL_MS
#define HEXDB_CACHE_TTL_MS (24UL * 60UL * 60UL * 1000UL)
#endif
#ifndef HEXDB_CACHE_SIZE
#define HEXDB_CACHE_SIZE 12
#endif
#ifndef HEXDB_FETCH_MIN_INTERVAL_MS
#define HEXDB_FETCH_MIN_INTERVAL_MS 15000
#endif
#ifndef HEXDB_MIN_HEAP
#define HEXDB_MIN_HEAP 50000
#endif
static HexDbCacheEntry g_hexdbCache[HEXDB_CACHE_SIZE];
static uint32_t g_hexdbLastFetchMs = 0;

struct RouteCacheEntry {
  String callsign;
  String route;
  uint32_t ts = 0;
};
static RouteCacheEntry g_routeCache;
static const size_t kMilLookupMax = 48;

bool flightEnrichmentIsMilitaryCached(const String &hex, bool &outIsMil) {
  uint32_t now = millis();
  for (size_t i = 0; i < kMilCacheSize; ++i) {
    if (g_milCache[i].hex == hex) {
      if (now - g_milCache[i].ts < 6UL * 60UL * 60UL * 1000UL) {
        outIsMil = g_milCache[i].isMil;
        return true;
      }
      return false;
    }
  }
  return false;
}

void flightEnrichmentStoreMilitary(const String &hex, bool isMil) {
  size_t slot = 0;
  uint32_t oldest = UINT32_MAX;
  for (size_t i = 0; i < kMilCacheSize; ++i) {
    if (g_milCache[i].hex.length() == 0) {
      slot = i;
      break;
    }
    if (g_milCache[i].ts < oldest) {
      oldest = g_milCache[i].ts;
      slot = i;
    }
  }
  g_milCache[slot].hex = hex;
  g_milCache[slot].ts = millis();
  g_milCache[slot].isMil = isMil;
}

bool flightEnrichmentFetchIsMilitary(const String &hex, bool &outIsMil) {
  if (WiFi.status() != WL_CONNECTED) return false;

  String url = String(API_BASE);
  if (url.startsWith("http://")) url.replace("http://", "https://");
  if (!url.startsWith("http")) url = String("https://") + url;
  url += "/v2/mil";

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10);
  HTTPClient http;
  http.setConnectTimeout(8000);
  http.setTimeout(10000);
  if (!http.begin(client, url)) return false;
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  Stream &stream = http.getStream();
  String hexLower = hex;
  hexLower.toLowerCase();
  String hexUpper = hex;
  hexUpper.toUpperCase();
  String needleLower = String("\"hex\":\"") + hexLower + "\"";
  String needleUpper = String("\"hex\":\"") + hexUpper + "\"";
  const char *countNeedle = "\"hex\":\"";
  const size_t countNeedleLen = strlen(countNeedle);
  uint32_t hexCount = 0;
  String tail;
  char buf[160];
  bool found = false;
  while (http.connected() || stream.available()) {
    int n = stream.readBytes(buf, sizeof(buf) - 1);
    if (n <= 0) break;
    buf[n] = '\0';
    String chunk = tail + String(buf);
    int scanStart = 0;
    if ((int)tail.length() >= (int)(countNeedleLen - 1)) {
      scanStart = (int)tail.length() - (int)(countNeedleLen - 1);
    }
    const char *chunkC = chunk.c_str();
    int maxStart = (int)chunk.length() - (int)countNeedleLen;
    for (int i = scanStart; i <= maxStart; ++i) {
      if (memcmp(chunkC + i, countNeedle, countNeedleLen) == 0) {
        ++hexCount;
        i += (int)countNeedleLen - 1;
      }
    }
    if (chunk.indexOf(needleLower) >= 0 || chunk.indexOf(needleUpper) >= 0) {
      found = true;
      break;
    }
    int keep = max(0, (int)max(needleLower.length(), needleUpper.length()) - 1);
    if (chunk.length() > keep) tail = chunk.substring(chunk.length() - keep);
    else tail = chunk;
    yield();
  }
  LOG_INFO("Mil list entries: %lu", (unsigned long)hexCount);
  http.end();
  outIsMil = found;
  return true;
}

bool flightEnrichmentFetchMilList(const String *hexes, size_t count, bool *outIsMil) {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (count == 0) return false;
  if (count > kMilLookupMax) return false;

  String url = String(API_BASE);
  if (url.startsWith("http://")) url.replace("http://", "https://");
  if (!url.startsWith("http")) url = String("https://") + url;
  url += "/v2/mil";

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10);
  HTTPClient http;
  http.setConnectTimeout(8000);
  http.setTimeout(10000);
  if (!http.begin(client, url)) return false;
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  Stream &stream = http.getStream();
  auto hexNibble = [](char c) -> int8_t {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  };
  auto parseHexString = [&](const String &s, uint32_t &out) -> bool {
    uint32_t val = 0;
    uint8_t digits = 0;
    const char *p = s.c_str();
    for (; *p; ++p) {
      int8_t nib = hexNibble(*p);
      if (nib < 0) continue;
      if (digits >= 6) break;
      val = (val << 4) | (uint32_t)nib;
      ++digits;
    }
    if (digits == 0) return false;
    out = val;
    return true;
  };

  uint32_t candVals[kMilLookupMax];
  for (size_t i = 0; i < count; ++i) {
    candVals[i] = 0;
    parseHexString(hexes[i], candVals[i]);
  }

  enum class ParseState : uint8_t { Search, InHex };
  static const char *kNeedle = "\"hex\":\"";
  static const uint8_t kNeedleLen = 7;
  ParseState state = ParseState::Search;
  uint8_t match = 0;
  uint32_t curHex = 0;
  uint8_t curDigits = 0;
  uint32_t hexCount = 0;
  size_t found = 0;
  bool done = false;

  auto consumeHex = [&]() {
    if (curDigits == 0) return;
    ++hexCount;
    for (size_t i = 0; i < count; ++i) {
      if (!outIsMil[i] && candVals[i] == curHex) {
        outIsMil[i] = true;
        ++found;
      }
    }
    curHex = 0;
    curDigits = 0;
  };

  char buf[160];
  while (!done && (http.connected() || stream.available())) {
    int n = stream.readBytes(buf, sizeof(buf));
    if (n <= 0) break;
    for (int i = 0; i < n; ++i) {
      char c = buf[i];
      if (state == ParseState::Search) {
        if (c == kNeedle[match]) {
          ++match;
          if (match == kNeedleLen) {
            state = ParseState::InHex;
            match = 0;
            curHex = 0;
            curDigits = 0;
          }
        } else {
          match = (c == kNeedle[0]) ? 1 : 0;
        }
      } else {
        if (c == '"') {
          consumeHex();
          state = ParseState::Search;
        } else {
          int8_t nib = hexNibble(c);
          if (nib >= 0) {
            if (curDigits < 6) {
              curHex = (curHex << 4) | (uint32_t)nib;
              ++curDigits;
            }
          } else {
            curHex = 0;
            curDigits = 0;
            state = ParseState::Search;
          }
        }
      }
    }
    if (found >= count) {
      done = true;
    }
    yield();
  }
  LOG_INFO("Mil list entries: %lu", (unsigned long)hexCount);
  http.end();

  for (size_t i = 0; i < count; ++i) {
    flightEnrichmentStoreMilitary(hexes[i], outIsMil[i]);
  }
  return true;
}

static bool hexDbCacheLookup(const String &hex, String &outName, String &outType,
                             String &outOwner) {
  uint32_t now = millis();
  for (size_t i = 0; i < HEXDB_CACHE_SIZE; ++i) {
    if (g_hexdbCache[i].hex == hex) {
      if (now - g_hexdbCache[i].ts < HEXDB_CACHE_TTL_MS) {
        outName = g_hexdbCache[i].name;
        outType = g_hexdbCache[i].icaoType;
        outOwner = g_hexdbCache[i].owner;
        return true;
      }
      return false;
    }
  }
  return false;
}

static void hexDbCacheStore(const String &hex, const String &name, const String &type,
                            const String &owner) {
  size_t slot = 0;
  uint32_t oldest = UINT32_MAX;
  for (size_t i = 0; i < HEXDB_CACHE_SIZE; ++i) {
    if (g_hexdbCache[i].hex.length() == 0) {
      slot = i;
      break;
    }
    if (g_hexdbCache[i].ts < oldest) {
      oldest = g_hexdbCache[i].ts;
      slot = i;
    }
  }
  g_hexdbCache[slot].hex = hex;
  g_hexdbCache[slot].name = name;
  g_hexdbCache[slot].icaoType = type;
  g_hexdbCache[slot].owner = owner;
  g_hexdbCache[slot].ts = millis();
}

bool flightEnrichmentLookupHexDb(const String &hex, String &outName, String &outType,
                                 String &outOwner) {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (!hex.length()) return false;
#if defined(ESP32)
  uint32_t heap = ESP.getFreeHeap();
  if (heap < HEXDB_MIN_HEAP) {
    LOG_WARN("HexDB skip: low heap (%u)", (unsigned)heap);
    return false;
  }
#endif
  uint32_t now = millis();
  if (hexDbCacheLookup(hex, outName, outType, outOwner)) {
    LOG_INFO("HexDB cache hit for %s", hex.c_str());
    return true;
  }
  if ((int32_t)(now - g_hexdbLastFetchMs) < (int32_t)HEXDB_FETCH_MIN_INTERVAL_MS) {
    return false;
  }

  g_hexdbLastFetchMs = now;

  String url = String("https://hexdb.io/api/v1/aircraft/") + hex;

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10);
  HTTPClient http;
  http.setConnectTimeout(8000);
  http.setTimeout(10000);
  if (!http.begin(client, url)) return false;
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();
  if (err) return false;

  String manufacturer = doc["Manufacturer"] | "";
  String type = doc["Type"] | "";
  String icaoType = doc["ICAOTypeCode"] | "";
  String owner = doc["RegisteredOwners"] | "";
  manufacturer.trim();
  type.trim();
  icaoType.trim();
  owner.trim();

  String name;
  if (manufacturer.length() && type.length()) name = manufacturer + String(" ") + type;
  else if (type.length()) name = type;
  else if (manufacturer.length()) name = manufacturer;

  outName = name;
  outType = icaoType;
  outOwner = owner;
  if (outName.length() || outType.length() || outOwner.length()) {
    hexDbCacheStore(hex, outName, outType, outOwner);
    return true;
  }
  return false;
}

bool flightEnrichmentLookupRoute(const String &callsign, double lat, double lon,
                                 String &outRoute) {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (!callsign.length()) return false;

  if (g_routeCache.callsign == callsign &&
      (millis() - g_routeCache.ts) < ROUTE_CACHE_TTL_MS &&
      g_routeCache.route.length()) {
    outRoute = g_routeCache.route;
    LOG_INFO("Route cache hit for %s", callsign.c_str());
    return true;
  }

  String url = String(API_BASE);
  if (url.startsWith("http://")) url.replace("http://", "https://");
  if (!url.startsWith("http")) url = String("https://") + url;
  url += "/api/0/routeset";

  JsonDocument req;
  JsonArray planes = req["planes"].to<JsonArray>();
  JsonObject p0 = planes.add<JsonObject>();
  p0["callsign"] = callsign;
  if (!isnan(lat)) p0["lat"] = lat;
  if (!isnan(lon)) p0["lng"] = lon;

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10);
  HTTPClient http;
  http.setConnectTimeout(8000);
  http.setTimeout(10000);
  if (!http.begin(client, url)) return false;
  http.addHeader("Content-Type", "application/json");
  String body;
  serializeJson(req, body);
  LOG_INFO("Route lookup POST %s callsign=%s", url.c_str(), callsign.c_str());
  int code = http.POST(body);
  LOG_INFO("Route lookup status: %d", code);
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  String resp = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, resp);
  if (err) return false;
  LOG_DEBUG("Route lookup JSON ok");

  String route;
  if (doc.is<JsonArray>()) {
    JsonArray arr = doc.as<JsonArray>();
    if (arr.size() > 0) {
      JsonVariant v = arr[0];
      if (v.is<const char *>()) route = String(v.as<const char *>());
      else if (v.is<JsonObject>()) {
        JsonObject o = v.as<JsonObject>();
        if (o["_airport_codes_iata"]) route = String(o["_airport_codes_iata"].as<const char *>());
        else if (o["route"]) route = String(o["route"].as<const char *>());
        else if (o["routes"]) route = String(o["routes"].as<const char *>());
      }
    }
  } else if (doc.is<JsonObject>()) {
    JsonObject o = doc.as<JsonObject>();
    if (o["_airport_codes_iata"]) route = String(o["_airport_codes_iata"].as<const char *>());
    else if (o["route"]) route = String(o["route"].as<const char *>());
    else if (o["routes"]) route = String(o["routes"].as<const char *>());
    else if (o["result"]) route = String(o["result"].as<const char *>());
  } else if (doc.is<const char *>()) {
    route = String(doc.as<const char *>());
  }

  route.trim();
  if (!route.length()) return false;
  String routeLower = route;
  routeLower.toLowerCase();
  if (routeLower == "unknown") return false;
  LOG_INFO("Route lookup result: %s", route.c_str());

  g_routeCache.callsign = callsign;
  g_routeCache.route = route;
  g_routeCache.ts = millis();
  outRoute = route;
  return true;
}

String flightEnrichmentClassifyOp(const FlightInfo &fi) {
  if (FEATURE_MIL_LOOKUP && fi.hex.length()) {
    bool isMil = false;
    if (flightEnrichmentIsMilitaryCached(fi.hex, isMil)) {
      if (isMil) return String("MIL");
    } else {
      bool ok = flightEnrichmentFetchIsMilitary(fi.hex, isMil);
      if (ok) flightEnrichmentStoreMilitary(fi.hex, isMil);
      if (ok && isMil) return String("MIL");
    }
  }

  if (fi.typeCode.length()) {
    uint16_t maxSeats = 0;
    if (aircraftSeatMax(fi.typeCode, maxSeats)) {
      if (maxSeats > 0 && maxSeats <= 20) {
        return String("PVT");
      }
    }
  }

  return fi.hasCallsign ? String("COM") : String("PVT");
}
