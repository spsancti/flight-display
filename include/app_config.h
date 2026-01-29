#pragma once

#include <Arduino.h>

#if __has_include("config.h")
#include "config.h"  // Create from example-config.h and do not commit secrets
#endif
#ifndef WIFI_SSID
#include "example-config.h"
#endif
