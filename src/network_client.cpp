#include "network_client.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "aircraft_types.h"
#include "app_config.h"
#include "config_features.h"
#include "flight_enrichment.h"
#include "flight_parser.h"
#include "log.h"

#ifndef FEATURE_HEXDB_LOOKUP
#define FEATURE_HEXDB_LOOKUP 1
#endif

static uint16_t radiusNmFromKm(double km) {
  if (km <= 0) return 0;
  double nm = km * 0.539957;
  uint16_t v = (uint16_t)(nm + 0.5);
  if (v == 0) v = 1;
  if (v > 250) v = 250;
  return v;
}

namespace {
constexpr size_t kMilCandidateMax = 48;

struct MilCandidate {
  FlightInfo fi;
  bool inFlight = false;
  bool isMil = false;
};

static MilCandidate g_milCands[kMilCandidateMax];
static String g_milFetchHexes[kMilCandidateMax];
static size_t g_milFetchMap[kMilCandidateMax];
static bool g_milFetchIsMil[kMilCandidateMax];
}  // namespace

bool networkClientFetchNearestFlight(FlightInfo &out) {
  if (WiFi.status() != WL_CONNECTED) return false;

  auto buildUrl = [](bool tls) {
    String base = String(API_BASE);
    if (tls) {
      if (base.startsWith("http://")) base.replace("http://", "https://");
      if (!base.startsWith("http")) base = String("https://") + base;
    } else {
      if (base.startsWith("https://")) base.replace("https://", "http://");
      if (!base.startsWith("http")) base = String("http://") + base;
    }
    base += "/v2/lat/";
    base += String(HOME_LAT, 6);
    base += "/lon/";
    base += String(HOME_LON, 6);
    base += "/dist/";
    base += String(radiusNmFromKm(SEARCH_RADIUS_KM));
    return base;
  };

  String url = buildUrl(true);
  LOG_INFO("HTTP GET %s", url.c_str());
  LOG_DEBUG("WiFi RSSI: %d dBm", WiFi.RSSI());
  LOG_DEBUG("Free heap: %u", (unsigned)ESP.getFreeHeap());

  HTTPClient http;
  http.setReuse(false);
  http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);
  http.setTimeout(HTTP_READ_TIMEOUT_MS);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  WiFiClientSecure client;
  client.setInsecure();
  uint32_t clientTimeoutSec = (HTTP_READ_TIMEOUT_MS + 999) / 1000;
  client.setTimeout(clientTimeoutSec);
  if (!http.begin(client, url)) {
    LOG_ERROR("HTTP begin failed (TLS)");
    return false;
  }
  http.addHeader("Accept", "application/json");
  http.addHeader("Accept-Encoding", "identity");
  http.addHeader("Connection", "close");
  http.addHeader("User-Agent", "ESP32-FlightDisplay/2.0");

  int code = http.GET();
  LOG_INFO("HTTP status: %d", code);
  if (code != HTTP_CODE_OK) {
    LOG_WARN("HTTP error: %s", http.errorToString(code).c_str());
    http.end();
    return false;
  }

  size_t contentLength = http.getSize();
  LOG_DEBUG("HTTP Content-Length: %u", (unsigned)contentLength);

  JsonDocument filter;
  JsonArray acArr = filter["ac"].to<JsonArray>();
  JsonObject acObj = acArr.add<JsonObject>();
  acObj["flight"] = true;
  acObj["r"] = true;
  acObj["hex"] = true;
  acObj["t"] = true;
  acObj["type"] = true;
  acObj["alt_baro"] = true;
  acObj["alt_geom"] = true;
  acObj["lat"] = true;
  acObj["lon"] = true;
  acObj["category"] = true;
  acObj["seen_pos"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream(),
                                             DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    LOG_WARN("JSON parse error (streamed): %s", err.c_str());
    return false;
  }

  JsonVariant root = doc.as<JsonVariant>();
  if (!root.is<JsonObject>() || !root.as<JsonObject>()["ac"].is<JsonArray>()) {
    LOG_INFO("No valid aircraft list in response");
    return false;
  }
  JsonArray ac = root.as<JsonObject>()["ac"].as<JsonArray>();

  FlightInfo bestAir;
  FlightInfo bestGround;
  FlightInfo bestMilAir;
  FlightInfo bestMilGround;
  bool hasAir = false;
  bool hasGround = false;
  bool hasMilAir = false;
  bool hasMilGround = false;

  size_t milCount = 0;
  bool milTruncated = false;

  for (JsonVariant v : ac) {
    if (!v.is<JsonObject>()) continue;
    FlightInfo fi;
    if (!flightParserParseAircraft(v.as<JsonObject>(), fi)) continue;
    bool inFlight = fi.altitudeFt > 0;
    if (inFlight) {
      if (!hasAir || fi.distanceKm < bestAir.distanceKm) {
        bestAir = fi;
        hasAir = true;
      }
    } else {
      if (!hasGround || fi.distanceKm < bestGround.distanceKm) {
        bestGround = fi;
        hasGround = true;
      }
    }
    if (fi.hex.length()) {
      if (milCount < kMilCandidateMax) {
        g_milCands[milCount].fi = fi;
        g_milCands[milCount].inFlight = inFlight;
        g_milCands[milCount].isMil = false;
        ++milCount;
      } else {
        milTruncated = true;
      }
    }
  }

  if (milTruncated) {
    LOG_WARN("MIL candidate list truncated at %u", (unsigned)kMilCandidateMax);
  }

  if (FEATURE_MIL_LOOKUP && milCount > 0) {
    size_t fetchCount = 0;

    for (size_t i = 0; i < milCount; ++i) {
      bool isMil = false;
      if (flightEnrichmentIsMilitaryCached(g_milCands[i].fi.hex, isMil)) {
        g_milCands[i].isMil = isMil;
      } else {
        g_milFetchHexes[fetchCount] = g_milCands[i].fi.hex;
        g_milFetchMap[fetchCount] = i;
        ++fetchCount;
      }
    }

    if (fetchCount > 0) {
      for (size_t i = 0; i < fetchCount; ++i) {
        g_milFetchIsMil[i] = false;
      }
      if (flightEnrichmentFetchMilList(g_milFetchHexes, fetchCount, g_milFetchIsMil)) {
        for (size_t i = 0; i < fetchCount; ++i) {
          g_milCands[g_milFetchMap[i]].isMil = g_milFetchIsMil[i];
        }
      }
    }

    for (size_t i = 0; i < milCount; ++i) {
      if (!g_milCands[i].isMil) continue;
      if (g_milCands[i].inFlight) {
        if (!hasMilAir || g_milCands[i].fi.distanceKm < bestMilAir.distanceKm) {
          bestMilAir = g_milCands[i].fi;
          hasMilAir = true;
        }
      } else {
        if (!hasMilGround || g_milCands[i].fi.distanceKm < bestMilGround.distanceKm) {
          bestMilGround = g_milCands[i].fi;
          hasMilGround = true;
        }
      }
    }
  }

  if (!hasAir && !hasGround) {
    LOG_INFO("No valid aircraft found in response");
    return false;
  }

  FlightInfo closest;
  if (hasMilAir) {
    closest = bestMilAir;
    LOG_INFO("Selected military airborne %s  dist %.2f km", closest.ident.c_str(),
             closest.distanceKm);
  } else {
    closest = hasAir ? bestAir : bestGround;
    if (hasAir) {
      LOG_INFO("Closest airborne %s  dist %.2f km", closest.ident.c_str(), closest.distanceKm);
    } else {
      LOG_INFO("Closest grounded %s  dist %.2f km", closest.ident.c_str(), closest.distanceKm);
    }
  }

  if (FEATURE_HEXDB_LOOKUP && closest.hex.length()) {
    bool typeKnown = closest.typeCode.length() && aircraftFriendlyName(closest.typeCode).length();
    bool needOwner = !closest.route.length();
    if (!typeKnown || needOwner) {
      String name;
      String icaoType;
      String owner;
      if (flightEnrichmentLookupHexDb(closest.hex, name, icaoType, owner)) {
        if (!typeKnown && icaoType.length()) {
          closest.typeCode = icaoType;
        }
        if (name.length()) closest.displayName = name;
        if (owner.length()) closest.registeredOwner = owner;
      }
    }
  }

  closest.opClass = flightEnrichmentClassifyOp(closest);
  LOG_INFO("Classified op: %s", closest.opClass.c_str());

  if (FEATURE_ROUTE_LOOKUP && closest.hasCallsign) {
    String route;
    if (flightEnrichmentLookupRoute(closest.ident, closest.lat, closest.lon, route)) {
      closest.route = route;
    } else {
      LOG_WARN("Route lookup failed for %s", closest.ident.c_str());
    }
  } else if (FEATURE_ROUTE_LOOKUP && !closest.hasCallsign) {
    LOG_INFO("Route lookup skipped: no callsign for %s", closest.ident.c_str());
  }

  out = closest;
  return true;
}
