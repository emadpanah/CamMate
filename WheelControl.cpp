#include <Arduino.h>
#include "WheelControl.h"

void WheelControl::begin(const WheelPins& pins, uint32_t pwmFreqHz, uint8_t pwmBits) {
  _p = pins;
  _bits = pwmBits;
  _freq = pwmFreqHz;

  // Direction pins
  pinMode(_p.in1, OUTPUT);
  pinMode(_p.in2, OUTPUT);
  pinMode(_p.in3, OUTPUT);
  pinMode(_p.in4, OUTPUT);

  // --- PWM configuration (ESP32 core 3.x syntax) ---
  analogWriteResolution(_p.enA, _bits);
  analogWriteResolution(_p.enB, _bits);
  analogWriteFrequency(_p.enA, _freq);
  analogWriteFrequency(_p.enB, _freq);

  // Safe idle
  coast();
  _ready = true;
}

int WheelControl::_clamp255(int v) const {
  if (v < -255) v = -255;
  if (v >  255) v =  255;
  return v;
}

int WheelControl::_toDuty(int val8) const {
  if (val8 < 0) val8 = 0;
  if (val8 > 255) val8 = 255;
  int maxDuty = (1 << _bits) - 1;
  return (val8 * maxDuty) / 255;
}

void WheelControl::_writePinDuty(uint8_t pin, int duty8) {
  analogWrite(pin, _toDuty(duty8));
}

void WheelControl::_applyDirLeft(bool fwd, uint8_t duty8) {
  digitalWrite(_p.in1, fwd ? HIGH : LOW);
  digitalWrite(_p.in2, fwd ? LOW  : HIGH);
  _writePinDuty(_p.enA, duty8);
}

void WheelControl::_applyDirRight(bool fwd, uint8_t duty8) {
  digitalWrite(_p.in3, fwd ? HIGH : LOW);
  digitalWrite(_p.in4, fwd ? LOW  : HIGH);
  _writePinDuty(_p.enB, duty8);
}

void WheelControl::setSpeedLeft(int spd) {
  if (!_ready) return;
  spd = _clamp255(spd);
  if (spd == 0) { coastLeft(); return; }
  bool fwd = (spd > 0);
  _applyDirLeft(fwd, (uint8_t)abs(spd));
}

void WheelControl::setSpeedRight(int spd) {
  if (!_ready) return;
  spd = _clamp255(spd);
  if (spd == 0) { coastRight(); return; }
  bool fwd = (spd > 0);
  _applyDirRight(fwd, (uint8_t)abs(spd));
}

void WheelControl::setSpeedBoth(int spd) {
  setSpeedLeft(spd);
  setSpeedRight(spd);
}

void WheelControl::brakeLeft() {
  if (!_ready) return;
  digitalWrite(_p.in1, HIGH);
  digitalWrite(_p.in2, HIGH);
  _writePinDuty(_p.enA, 0);
}

void WheelControl::brakeRight() {
  if (!_ready) return;
  digitalWrite(_p.in3, HIGH);
  digitalWrite(_p.in4, HIGH);
  _writePinDuty(_p.enB, 0);
}

void WheelControl::brake() {
  brakeLeft();
  brakeRight();
}

void WheelControl::coastLeft() {
  if (!_ready) return;
  digitalWrite(_p.in1, LOW);
  digitalWrite(_p.in2, LOW);
  _writePinDuty(_p.enA, 0);
}

void WheelControl::coastRight() {
  if (!_ready) return;
  digitalWrite(_p.in3, LOW);
  digitalWrite(_p.in4, LOW);
  _writePinDuty(_p.enB, 0);
}

void WheelControl::coast() {
  coastLeft();
  coastRight();
}
