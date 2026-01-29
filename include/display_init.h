#pragma once

#include <Arduino_GFX_Library.h>
#include "app_types.h"
#include "display/drivers/AmoledDisplay/Amoled_DisplayPanel.h"

bool displayInit();
DisplayState displayGetState();
DisplayMetrics displayGetMetrics();
Amoled_DisplayPanel &displayPanel();
Arduino_GFX *displayGfx();
bool displayIsReady();
void displaySetBrightness(uint8_t level);
