#pragma once
#include <ESP32Servo.h>

class ServoControl {
public:
  ServoControl();
  bool attach(int pin);
  void detach();
  bool attached();   

  void center();
  void writeDeg(int deg);
  int  readDeg() const;

  void slowMoveTo(int targetDeg, int stepDeg = 1, int stepDelayMs = 10);
  void sweep(int fromDeg, int toDeg, int stepDeg = 1, int stepDelayMs = 10);

private:
  Servo _servo;
  int   _pin = -1;
  int   _lastDeg = -1;
  int   _clampDeg(int d) const;
};
