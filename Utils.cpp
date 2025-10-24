#include "Utils.h"
#include "config.h"

void printMenu() {
  Serial.println();
  Serial.println(F("=== CamMate Servo Test Menu ==="));
  Serial.println(F("Single-key (no Enter):"));
  Serial.println(F("  f          -> select FRONT servo"));
  Serial.println(F("  r          -> select REAR  servo"));
  Serial.println(F("  n          -> move selected servo -10°"));
  Serial.println(F("  m          -> move selected servo +10°"));
  Serial.println(F(""));
  Serial.println(F("Line commands (press Enter):"));
  Serial.println(F("  A90        -> set BOTH servos to 90°"));
  Serial.println(F("  c          -> center both (90°)"));
  Serial.println(F("  sf         -> sweep FRONT 60..120"));
  Serial.println(F("  sr         -> sweep REAR  60..120"));
  Serial.println(F("  bf=ANG     -> set BOTH to ANG (0..180)"));
  Serial.println(F("  ff=ANG     -> set FRONT to ANG"));
  Serial.println(F("  fr=ANG     -> set REAR  to ANG"));
  Serial.println(F("  mf=ANG     -> slow move FRONT to ANG"));
  Serial.println(F("  mr=ANG     -> slow move REAR  to ANG"));
  Serial.println(F("  help       -> show this menu"));
  Serial.println();
}

bool readLine(Stream& s, String& out) {
  while (s.available()) {
    char ch = (char)s.read();
    if (ch == '\r') continue;
    if (ch == '\n') return true;
    out += ch;
  }
  return false;
}

int parseAngle(const String& s, int fallback) {
  int val = s.toInt();
  if (val == 0 && s != "0") return fallback;
  if (val < SERVO_MIN_DEG) val = SERVO_MIN_DEG;
  if (val > SERVO_MAX_DEG) val = SERVO_MAX_DEG;
  return val;
}
