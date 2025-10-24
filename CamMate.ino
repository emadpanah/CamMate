/*
  CamMate – Servo + Wheel bring-up
  - Board: ESP32 Dev Module (ESP32-WROOM-32)
  - Libraries: ESP32Servo
  - Wheels: L298N (PWM on ENA/ENB; remove ENA/ENB jumpers)
  Serial Monitor: 115200 baud
*/

#include <Arduino.h>
#include "config.h"
#include "ServoControl.h"
#include "WheelControl.h"
#include "Utils.h"

ServoControl servoFront;
ServoControl servoRear;
WheelControl wheels;

enum SelectedServo { SEL_FRONT = 0, SEL_REAR = 1 };
SelectedServo selected = SEL_FRONT;

static inline int clampDeg(int d) {
  if (d < SERVO_MIN_DEG) d = SERVO_MIN_DEG;
  if (d > SERVO_MAX_DEG) d = SERVO_MAX_DEG;
  return d;
}

void selectFront() { selected = SEL_FRONT; Serial.println(F("[SELECT] Front servo selected")); }
void selectRear()  { selected = SEL_REAR;  Serial.println(F("[SELECT] Rear servo selected"));  }

void moveSelectedBy(int delta) {
  if (selected == SEL_FRONT) {
    int cur = servoFront.readDeg(); if (cur < 0) cur = SERVO_CENTER;
    int nxt = clampDeg(cur + delta);
    servoFront.writeDeg(nxt);
    Serial.printf("[MOVE] Front: %d -> %d (delta %+d)\n", cur, nxt, delta);
  } else {
    int cur = servoRear.readDeg(); if (cur < 0) cur = SERVO_CENTER;
    int nxt = clampDeg(cur + delta);
    servoRear.writeDeg(nxt);
    Serial.printf("[MOVE] Rear : %d -> %d (delta %+d)\n", cur, nxt, delta);
  }
}

void setAllTo(int ang) {
  int a = clampDeg(ang);
  servoFront.writeDeg(a);
  servoRear.writeDeg(a);
  Serial.printf("[ALL] Both servos set to %d°\n", a);
}

void printWheelHelp() {
  Serial.println(F("=== Wheel Commands ==="));
  Serial.println(F("  whelp       -> show this help"));
  Serial.println(F("  wstop       -> coast (free-roll)"));
  Serial.println(F("  wbrake      -> brake both"));
  Serial.println(F("  wl=VAL      -> left speed  (-255..255)"));
  Serial.println(F("  wr=VAL      -> right speed (-255..255)"));
  Serial.println(F("  ws=VAL      -> both speeds (-255..255)"));
  Serial.println(F("  wf=MS       -> forward  WHEEL_DEFAULT_SPEED for MS ms, then brake"));
  Serial.println(F("  wb=MS       -> backward WHEEL_DEFAULT_SPEED for MS ms, then brake"));
  Serial.println(F("  wrl=MS      -> rotate left  (L=-V, R=+V) for MS ms, then brake"));
  Serial.println(F("  wrr=MS      -> rotate right (L=+V, R=-V) for MS ms, then brake"));
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(400);
  Serial.println(F("\n=== CamMate v2 – Servo + Wheel Test ==="));
  Serial.println(F("Baud 115200. Single keys f/r/n/m for servo jog; type 'help' or 'whelp' for menus."));
  Serial.println(F("NOTE: Remove ENA/ENB jumpers on L298N for PWM. Ensure common GND."));

  // Servos
  bool okF = servoFront.attach(SERVO_FRONT_PIN);
  bool okR = servoRear.attach(SERVO_REAR_PIN);
  if (!okF || !okR) Serial.println(F("[ERROR] Servo attach failed. Check pins/power."));
  servoFront.center(); servoRear.center();
  selectFront();

  // Wheels
  WheelPins p{ L298_IN1, L298_IN2, L298_ENA, L298_IN3, L298_IN4, L298_ENB };
  wheels.begin(p, WHEEL_PWM_FREQ_HZ, WHEEL_PWM_BITS);
  Serial.println(F("[OK] Wheels ready."));

  printMenu();
  printWheelHelp();
}

void handleInstantKey(char k) {
  switch (k) {
    case 'f': selectFront(); break;
    case 'r': selectRear();  break;
    case 'n': moveSelectedBy(-10); break;
    case 'm': moveSelectedBy(+10); break;
    default: break;
  }
}

void processWheelCommand(const String& cmd) {
  if (cmd == "whelp") {
    printWheelHelp();
  } else if (cmd == "wstop") {
    wheels.coast(); Serial.println(F("[WHEEL] coast"));
  } else if (cmd == "wbrake") {
    wheels.brake(); Serial.println(F("[WHEEL] brake"));
  } else if (cmd.startsWith("wl=")) {
    int v = cmd.substring(3).toInt();
    wheels.setSpeedLeft(v);
    Serial.printf("[WHEEL] left=%d\n", v);
  } else if (cmd.startsWith("wr=")) {
    int v = cmd.substring(3).toInt();
    wheels.setSpeedRight(v);
    Serial.printf("[WHEEL] right=%d\n", v);
  } else if (cmd.startsWith("ws=")) {
    int v = cmd.substring(3).toInt();
    wheels.setSpeedBoth(v);
    Serial.printf("[WHEEL] both=%d\n", v);
  } else if (cmd.startsWith("wf=")) {
    int ms = (int)max(0L, cmd.substring(3).toInt());   // <-- fixed
    Serial.printf("[WHEEL] forward %d ms at %d\n", ms, WHEEL_DEFAULT_SPEED);
    wheels.setSpeedBoth(WHEEL_DEFAULT_SPEED);
    delay(ms);
    wheels.brake();
  } else if (cmd.startsWith("wb=")) {
    int ms = (int)max(0L, cmd.substring(3).toInt());   // <-- fixed
    Serial.printf("[WHEEL] backward %d ms at %d\n", ms, WHEEL_DEFAULT_SPEED);
    wheels.setSpeedBoth(-WHEEL_DEFAULT_SPEED);
    delay(ms);
    wheels.brake();
  } else if (cmd.startsWith("wrl=")) {
    int ms = (int)max(0L, cmd.substring(4).toInt());   // <-- fixed
    Serial.printf("[WHEEL] rotate left %d ms at %d\n", ms, WHEEL_DEFAULT_SPEED);
    wheels.setSpeedLeft(-WHEEL_DEFAULT_SPEED);
    wheels.setSpeedRight(+WHEEL_DEFAULT_SPEED);
    delay(ms);
    wheels.brake();
  } else if (cmd.startsWith("wrr=")) {
    int ms = (int)max(0L, cmd.substring(4).toInt());   // <-- fixed
    Serial.printf("[WHEEL] rotate right %d ms at %d\n", ms, WHEEL_DEFAULT_SPEED);
    wheels.setSpeedLeft(+WHEEL_DEFAULT_SPEED);
    wheels.setSpeedRight(-WHEEL_DEFAULT_SPEED);
    delay(ms);
    wheels.brake();
  } else {
    Serial.println(F("[WHEEL] Unknown. Type 'whelp'."));
  }
}

void processLineCommand(const String& cmd) {
  // Existing servo commands
  if (cmd == "c") {
    servoFront.center(); servoRear.center();
    Serial.println(F("[CMD] Both centered (90°)."));
  }
  else if (cmd == "sf") {
    Serial.println(F("[CMD] Sweeping FRONT 60..120"));
    servoFront.sweep(60, 120, SERVO_DEFAULT_SPEED_DEG_PER_STEP, SERVO_DEFAULT_STEP_DELAY_MS);
  }
  else if (cmd == "sr") {
    Serial.println(F("[CMD] Sweeping REAR 60..120"));
    servoRear.sweep(60, 120, SERVO_DEFAULT_SPEED_DEG_PER_STEP, SERVO_DEFAULT_STEP_DELAY_MS);
  }
  else if (cmd.startsWith("bf=")) {
    int ang = parseAngle(cmd.substring(3), SERVO_CENTER);
    servoFront.writeDeg(ang); servoRear.writeDeg(ang);
    Serial.printf("[CMD] Both set to %d°\n", ang);
  }
  else if (cmd.startsWith("ff=")) {
    int ang = parseAngle(cmd.substring(3), SERVO_CENTER);
    servoFront.writeDeg(ang);
    Serial.printf("[CMD] Front set to %d°\n", ang);
  }
  else if (cmd.startsWith("fr=")) {
    int ang = parseAngle(cmd.substring(3), SERVO_CENTER);
    servoRear.writeDeg(ang);
    Serial.printf("[CMD] Rear set to %d°\n", ang);
  }
  else if (cmd.startsWith("mf=")) {
    int ang = parseAngle(cmd.substring(3), SERVO_CENTER);
    Serial.printf("[CMD] Front slowMoveTo %d°\n", ang);
    servoFront.slowMoveTo(ang, SERVO_DEFAULT_SPEED_DEG_PER_STEP, SERVO_DEFAULT_STEP_DELAY_MS);
  }
  else if (cmd.startsWith("mr=")) {
    int ang = parseAngle(cmd.substring(3), SERVO_CENTER);
    Serial.printf("[CMD] Rear slowMoveTo %d°\n", ang);
    servoRear.slowMoveTo(ang, SERVO_DEFAULT_SPEED_DEG_PER_STEP, SERVO_DEFAULT_STEP_DELAY_MS);
  }
  else if (cmd.equalsIgnoreCase("A90")) {
    setAllTo(90);
  }
  // Wheel commands
  else if (cmd.length() && cmd[0] == 'w') {
    processWheelCommand(cmd);
  }
  else if (cmd == "help") {
    printMenu();
    printWheelHelp();
  }
  else {
    Serial.println(F("[WARN] Unknown command. Type 'help' or 'whelp'."));
  }
}

void loop() {
  // Instant servo jog keys
  while (Serial.available()) {
    int pk = Serial.peek();
    if (pk == '\n' || pk == '\r') { Serial.read(); continue; }
    if (pk == 'f' || pk == 'r' || pk == 'n' || pk == 'm') {
      char k = (char)Serial.read();
      handleInstantKey(k);
    } else {
      break;
    }
  }

  // Line-based commands (including wheel cmds)
  static String cmd;
  if (readLine(Serial, cmd)) {
    cmd.trim();
    if (cmd.length() > 0) processLineCommand(cmd);
    cmd = "";
  }
}
