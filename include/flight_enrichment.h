#pragma once

#include "app_types.h"

bool flightEnrichmentIsMilitaryCached(const String &hex, bool &outIsMil);
void flightEnrichmentStoreMilitary(const String &hex, bool isMil);
bool flightEnrichmentFetchIsMilitary(const String &hex, bool &outIsMil);
bool flightEnrichmentFetchMilList(const String *hexes, size_t count, bool *outIsMil);

bool flightEnrichmentLookupHexDb(const String &hex, String &outName, String &outType,
                                 String &outOwner);

bool flightEnrichmentLookupRoute(const String &callsign, double lat, double lon,
                                 String &outRoute);

String flightEnrichmentClassifyOp(const FlightInfo &fi);
