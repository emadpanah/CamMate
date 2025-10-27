#include "Speed.h"

// Default to NORMAL at boot
SpeedMode g_speedMode = SPEED_NORMAL;

static inline float clamp01f(float v){
  if (v < 0) return 0;
  if (v > 1) return 1;
  return v;
}

int applySpeedScaling(int basePwm, float steerExtent) {
  // Slow down as steering approaches extremes
  float steerScale = 1.0f - (SPEED_STEER_SCALE * clamp01f(steerExtent));

  // Speed mode multiplier
  float modeScale = 1.0f;
  switch (g_speedMode) {
    case SPEED_SPORT:  modeScale = 1.0f;  break;  // 100%
    case SPEED_NORMAL: modeScale = 0.5f;  break;  // 50%
    case SPEED_LOW:    modeScale = 0.25f; break;  // 25%
  }

  float v = basePwm * steerScale * modeScale;
  if (v >  255) v =  255;
  if (v < -255) v = -255;
  return (int)v;
}
