#include "MotionPlanner.h"
#include <math.h>

static inline float clamp11f(float v){
  if (v < -1.0f) return -1.0f;
  if (v >  1.0f) return  1.0f;
  return v;
}
static inline float clamp01f(float v){
  if (v < 0.0f) return 0.0f;
  if (v > 1.0f) return 1.0f;
  return v;
}

void planSteering(float x, float y, UIMode mode, float diam,
                  int &ff, int &fr, int &base, float &steerExtent)
{
  x = clamp11f(x);
  y = clamp11f(y);
  diam = clamp01f(diam);

  base = (int)(y * 255.0f);

  float ffF = SERVO_CENTER;
  float frF = SERVO_CENTER;

  switch (mode) {
    case MODE_NORMAL: {
      // Ackermann-ish: opposite steer front/back
      float ax = fabsf(x);
      if (x < 0) { // LEFT
        ffF = mp_mixf((float)SERVO_CENTER, (float)FF_MIN, ax); // 90..60
        frF = mp_mixf((float)SERVO_CENTER, (float)FR_MAX, ax); // 90..120
      } else {     // RIGHT
        ffF = mp_mixf((float)SERVO_CENTER, (float)FF_MAX, ax); // 90..140
        frF = mp_mixf((float)SERVO_CENTER, (float)FR_MIN, ax); // 90..40
      }
      steerExtent = ax;
    } break;

    case MODE_CRAB: {
      // Parallel steer (both to same side)
      float ax = fabsf(x);
      if (x >= 0) { // CRAB RIGHT
        ffF = mp_mixf((float)SERVO_CENTER, (float)FF_MAX, ax); // 90..140
        frF = mp_mixf((float)SERVO_CENTER, (float)FR_MAX, ax); // 90..120
      } else {      // CRAB LEFT
        ffF = mp_mixf((float)SERVO_CENTER, (float)FF_MIN, ax); // 90..60
        frF = mp_mixf((float)SERVO_CENTER, (float)FR_MIN, ax); // 90..40
      }
      steerExtent = ax;
    } break;

    case MODE_CIRCLE: {
      // diam=0 -> minimum radius using extremes; diam=1 -> straight (90/90)
      float t = diam;
      if (x < 0) { // Circle LEFT
        ffF = mp_mixf((float)FF_MIN, (float)SERVO_CENTER, t); // 60..90
        frF = mp_mixf((float)FR_MAX, (float)SERVO_CENTER, t); // 120..90
      } else {     // Circle RIGHT
        ffF = mp_mixf((float)FF_MAX, (float)SERVO_CENTER, t); // 140..90
        frF = mp_mixf((float)FR_MIN, (float)SERVO_CENTER, t); // 40..90
      }
      steerExtent = 1.0f - t;
    } break;

    default:
      steerExtent = 0.0f;
      break;
  }

  ff = mp_clampInt((int)lroundf(ffF), FF_MIN, FF_MAX);
  fr = mp_clampInt((int)lroundf(frF), FR_MIN, FR_MAX);
}
