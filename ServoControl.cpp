#include "ServoControl.h"
#include "config.h"
#include <Arduino.h>

ServoControl::ServoControl() {}

bool ServoControl::attach(int pin) {
  _pin = pin;
  bool ok = _servo.attach(_pin, 500, 2400); // 50Hz, wide pulse range
  if (ok) center();
  return ok;
}

void ServoControl::detach() { _servo.detach(); _pin = -1; }

bool ServoControl::attached() {                
  return _servo.attached();
}

void ServoControl::center() { writeDeg(SERVO_CENTER); }

void ServoControl::writeDeg(int deg) {
  int d = _clampDeg(deg);
  _servo.write(d);
  _lastDeg = d;
}

int ServoControl::readDeg() const { return _lastDeg; }

void ServoControl::slowMoveTo(int targetDeg, int stepDeg, int stepDelayMs) {
  if (!attached()) return;
  int current = (_lastDeg < 0) ? SERVO_CENTER : _lastDeg;
  int target  = _clampDeg(targetDeg);
  int step    = (target >= current) ? abs(stepDeg) : -abs(stepDeg);

  for (int d = current; (step > 0) ? (d <= target) : (d >= target); d += step) {
    _servo.write(d);
    _lastDeg = d;
    delay(stepDelayMs);
  }
}

void ServoControl::sweep(int fromDeg, int toDeg, int stepDeg, int stepDelayMs) {
  int a = _clampDeg(fromDeg), b = _clampDeg(toDeg);
  if (a > b) { int t = a; a = b; b = t; }
  for (int d = a; d <= b; d += abs(stepDeg)) { _servo.write(d); _lastDeg = d; delay(stepDelayMs); }
  for (int d = b; d >= a; d -= abs(stepDeg)) { _servo.write(d); _lastDeg = d; delay(stepDelayMs); }
}

int ServoControl::_clampDeg(int d) const {
  if (d < SERVO_MIN_DEG) d = SERVO_MIN_DEG;
  if (d > SERVO_MAX_DEG) d = SERVO_MAX_DEG;
  return d;
}
