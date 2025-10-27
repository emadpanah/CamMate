#pragma once
#include <Arduino.h>
#include "config.h"

// Computes steering (ff, fr), base speed (from joyY), and steerExtent (0..1)
// Inputs: x (-1..+1), y (-1..+1), mode, diam (0..1)
// Outputs: ff, fr (degrees), base (-255..+255), steerExtent (0..1)
void planSteering(float x, float y, UIMode mode, float diam,
                  int &ff, int &fr, int &base, float &steerExtent);
