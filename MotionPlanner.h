#pragma once
#include <Arduino.h>
#include "config.h"

// Mix helper (linear interpolation)
static inline float mp_mixf(float a, float b, float t){ return a + (b - a) * t; }
static inline int   mp_clampInt(int v, int lo, int hi){ return (v<lo)?lo:((v>hi)?hi:v); }

// Compute steering + speed base from joystick state.
// Inputs:
//   x: -1..+1 (steer)
//   y: -1..+1 (throttle)
//   mode: MODE_NORMAL / MODE_CRAB / MODE_CIRCLE
//   diam: 0..1 for circle (0=tightest, 1=straight)
// Outputs (by ref):
//   ff, fr: servo target degrees (clamped to FF_/FR_ limits)
//   base:   base PWM from throttle (-255..+255)
//   steerExtent: 0..1 amount of steering for speed scaling
void planSteering(float x, float y, UIMode mode, float diam,
                  int &ff, int &fr, int &base, float &steerExtent);
