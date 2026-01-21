/**
 * @file      LV_Helper.h
 * @author    Lewis He (lewishe@outlook.com)
 * @license   MIT
 * @date      2024-01-22
 *
 */
#pragma once
#include "Display.h"
#include <Arduino.h>
#include <lvgl.h>

void beginLvglHelper(Display &board, bool debug = false);
String lvgl_helper_get_fs_filename(String filename);
const char *lvgl_helper_get_fs_filename(const char *filename);
