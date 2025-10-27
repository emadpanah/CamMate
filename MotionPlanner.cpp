#include "MotionPlanner.h"

static inline int   clampInt(int v, int lo, int hi){ return (v<lo)?lo:((v>hi)?hi:v); }
static inline float clamp01(float v){ if(v<0) return 0; if(v>1) return 1; return v; }
static inline float clamp11(float v){ if(v<-1) return -1; if(v>1) return 1; return v; }
static inline float mixf(float a, float b, float t){ return a + (b - a) * t; }

void planSteering(float x, float y, UIMode mode, float diam,
                  int &ff, int &fr, int &base, float &steerExtent)
{
  x = clamp11(x); y = clamp11(y); diam = clamp01(diam);

  base = (int)(y * 255.0f);
  steerExtent = 0.0f;

  float ffF = SERVO_CENTER;
  float frF = SERVO_CENTER;

  switch (mode) {
    case MODE_NORMAL: {
      float ax = fabsf(x);
      if (x < 0) { // LEFT
        ffF = mixf(90.0f,  60.0f, ax);
        frF = mixf(90.0f, 120.0f, ax);
      } else {     // RIGHT
        ffF = mixf(90.0f, 140.0f, ax);
        frF = mixf(90.0f,  40.0f, ax);
      }
      steerExtent = ax;
    } break;

    case MODE_CRAB: {
      float ax = fabsf(x);
      if (x >= 0) { // CRAB RIGHT
        ffF = mixf(90.0f, 140.0f, ax);
        frF = mixf(90.0f, 120.0f, ax);
      } else {      // CRAB LEFT
        ffF = mixf(90.0f,  60.0f, ax);
        frF = mixf(90.0f,  40.0f, ax);
      }
      steerExtent = ax;
    } break;

    case MODE_CIRCLE: {
      float t = diam; // 0..1 (1 is straight)
      if (x < 0) { // circle LEFT
        ffF = mixf( 60.0f, 90.0f, t);
        frF = mixf(120.0f, 90.0f, t);
      } else {     // circle RIGHT
        ffF = mixf(140.0f, 90.0f, t);
        frF = mixf( 40.0f, 90.0f, t);
      }
      steerExtent = 1.0f - t;
    } break;
  }

  ff = clampInt((int)roundf(ffF), FF_MIN, FF_MAX);
  fr = clampInt((int)roundf(frF), FR_MIN, FR_MAX);
}
