#pragma once

#include "app_types.h"

struct FlightState {
  FlightInfo latest;
  bool valid = false;
  uint32_t seq = 0;
};
