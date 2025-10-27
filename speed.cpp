#include "Speed.h"

volatile SpeedMode g_speedMode = SPEED_NORMAL;

static inline int clampInt(int v, int lo, int hi){ return (v<lo)?lo:((v>hi)?hi:v); }

int applySpeedScaling(int base, float steerExtent)
{
  // Base steering reduction
  float scale = 1.0f - (SPEED_STEER_SCALE * steerExtent);

  // Mode scaling:
  // Normal = 50%, Sport = 100%, Low = 50% of Normal
  float modeMul = 1.0f;
  int cap = MAX_SPEED_SPORT;

  switch (g_speedMode) {
    case SPEED_SPORT:   modeMul = 1.0f;   cap = MAX_SPEED_SPORT;  break;
    case SPEED_NORMAL:  modeMul = 0.5f;   cap = MAX_SPEED_NORMAL; break;
    case SPEED_LOW:     modeMul = 0.5f * SPEED_LOW_FACTOR; cap = MAX_SPEED_NORMAL; break;
  }

  int spd = (int)(base * scale * modeMul);
  spd = clampInt(spd, -cap, cap);
  return spd;
}
