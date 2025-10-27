#pragma once
#include <Arduino.h>
#include "config.h"

// 3 speed levels:
// - SPORT  = 100%
// - NORMAL = 50%
// - LOW    = 25% (half of normal)
enum SpeedMode : uint8_t { SPEED_LOW=0, SPEED_NORMAL=1, SPEED_SPORT=2 };

// Global current speed mode (defined in Speed.cpp)
extern SpeedMode g_speedMode;

// Apply steering-based slowdown + speed mode scaling.
// basePwm: -255..+255 (from throttle)
// steerExtent: 0..1 (0 = straight, 1 = max steering)
int applySpeedScaling(int basePwm, float steerExtent);
