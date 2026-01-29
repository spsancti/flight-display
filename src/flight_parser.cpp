#include "flight_parser.h"

#include <Arduino.h>
#include <math.h>

#include "app_config.h"
#include "config_features.h"

static double deg2rad(double deg) {
  return deg * PI / 180.0;
}

static double haversineKm(double lat1, double lon1, double lat2, double lon2) {
  const double R = 6371.0;
  double dLat = deg2rad(lat2 - lat1);
  double dLon = deg2rad(lon2 - lon1);
  double a = sin(dLat / 2) * sin(dLat / 2) +
             cos(deg2rad(lat1)) * cos(deg2rad(lat2)) *
                 sin(dLon / 2) * sin(dLon / 2);
  double c = 2 * atan2(sqrt(a), sqrt(1 - a));
  return R * c;
}

bool flightParserExtractLatLon(JsonObject obj, double &outLat, double &outLon) {
  if (obj["seen_pos"].is<double>()) {
    double seenPos = obj["seen_pos"].as<double>();
    if (seenPos > POSITION_MAX_AGE_S) return false;
  }
  if (obj["lat"].is<double>() && obj["lon"].is<double>()) {
    outLat = obj["lat"].as<double>();
    outLon = obj["lon"].as<double>();
    if (!(outLat == 0.0 && outLon == 0.0)) return true;
  }
  return false;
}

bool flightParserParseAircraft(JsonObject obj, FlightInfo &res) {
  double lat, lon;
  if (!flightParserExtractLatLon(obj, lat, lon)) return false;

  String ident;
  bool hasCallsign = false;
  if (obj["flight"].is<const char *>()) {
    ident = String(obj["flight"].as<const char *>());
    hasCallsign = ident.length() > 0;
  } else if (obj["r"].is<const char *>()) {
    ident = String(obj["r"].as<const char *>());
  } else if (obj["hex"].is<const char *>()) {
    ident = String(obj["hex"].as<const char *>());
  } else {
    ident = String("(unknown)");
  }
  ident.trim();

  long alt = -1;
  if (!obj["alt_baro"].isNull()) {
    alt = obj["alt_baro"].as<long>();
  } else if (!obj["alt_geom"].isNull()) {
    alt = obj["alt_geom"].as<long>();
  }

  String type = obj["t"].is<const char *>() ? String(obj["t"].as<const char *>()) : String("");
  if (!obj["t"].is<const char *>() && obj["type"].is<const char *>()) {
    type = String(obj["type"].as<const char *>());
  }
  String cat = obj["category"].is<const char *>() ? String(obj["category"].as<const char *>())
                                                  : String("");

  res.valid = true;
  res.ident = ident;
  res.typeCode = type;
  res.category = cat;
  res.altitudeFt = alt;
  res.lat = lat;
  res.lon = lon;
  res.distanceKm = haversineKm(HOME_LAT, HOME_LON, lat, lon);
  res.hex = obj["hex"].is<const char *>() ? String(obj["hex"].as<const char *>()) : String("");
  res.hasCallsign = hasCallsign;
  res.route = String("");
  res.displayName = String("");
  res.registeredOwner = String("");
  return true;
}

FlightInfo flightParserParseClosest(JsonVariant root) {
  FlightInfo res;
  if (!root.is<JsonObject>()) return res;
  JsonObject robj = root.as<JsonObject>();
  if (!robj["ac"].is<JsonArray>()) return res;
  JsonArray ac = robj["ac"].as<JsonArray>();
  if (ac.size() == 0) return res;
  JsonObject obj = ac[0].as<JsonObject>();
  if (!flightParserParseAircraft(obj, res)) return FlightInfo{};
  return res;
}
