#pragma once
#include <Arduino.h>

void printMenu();
bool readLine(Stream& s, String& out);
int  parseAngle(const String& s, int fallback);
