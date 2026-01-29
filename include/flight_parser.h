#pragma once

#include <ArduinoJson.h>
#include "app_types.h"

bool flightParserExtractLatLon(JsonObject obj, double &outLat, double &outLon);
bool flightParserParseAircraft(JsonObject obj, FlightInfo &out);
FlightInfo flightParserParseClosest(JsonVariant root);
