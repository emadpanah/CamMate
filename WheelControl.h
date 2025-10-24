#pragma once
#include <Arduino.h>

struct WheelPins {
  // Left motor (A)
  uint8_t in1, in2, enA; // enA = PWM
  // Right motor (B)
  uint8_t in3, in4, enB; // enB = PWM
};

class WheelControl {
public:
  WheelControl() {}

  void begin(const WheelPins& pins, uint32_t pwmFreqHz, uint8_t pwmBits);

  // Speed: -255..255 (sign = direction)
  void setSpeedLeft(int spd);
  void setSpeedRight(int spd);
  void setSpeedBoth(int spd);

  // Actions
  void brakeLeft();
  void brakeRight();
  void brake();
  void coastLeft();
  void coastRight();
  void coast();

private:
  WheelPins _p{};
  bool _ready = false;

  uint8_t _bits = 10;   // analogWriteResolution
  uint32_t _freq = 10000;

  int  _clamp255(int v) const;
  int  _toDuty(int val8) const; // map 0..255 -> 0..(2^bits-1)
  void _writePinDuty(uint8_t pin, int duty8);
  void _applyDirLeft(bool fwd, uint8_t duty8);
  void _applyDirRight(bool fwd, uint8_t duty8);
};
