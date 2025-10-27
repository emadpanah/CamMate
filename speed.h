#pragma once
#include <Arduino.h>
#include "config.h"

enum SpeedMode : uint8_t { SPEED_LOW=0, SPEED_NORMAL=1, SPEED_SPORT=2 };
extern volatile SpeedMode g_speedMode;

// Apply steering-based scaling + mode scaling + caps.
// base is -255..+255, steerExtent 0..1
int applySpeedScaling(int base, float steerExtent);
