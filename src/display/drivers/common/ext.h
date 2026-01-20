#pragma once
#ifndef _DRIVER_EXTENSION_H_
#define _DRIVER_EXTENSION_H_

#ifndef AMOLED_TOUCH_ENABLED
#define AMOLED_TOUCH_ENABLED 0
#endif

#if AMOLED_TOUCH_ENABLED
#include <TouchDrvCSTXXX.hpp>
#include <TouchDrvFT6X36.hpp>
#include <TouchDrvGT911.hpp>
#endif
#endif
