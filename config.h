#pragma once

// === Servo pins ===
#define SERVO_FRONT_PIN 18
#define SERVO_REAR_PIN  19

// === L298N pins (ESP32) ===
#define L298_ENA 25
#define L298_IN1 26
#define L298_IN2 27
#define L298_ENB 33
#define L298_IN3 32
#define L298_IN4 4

// === Servo ranges ===
#define SERVO_MIN_DEG  0
#define SERVO_MAX_DEG  180
#define SERVO_CENTER   90

// === Motion tuning (servos) ===
#define SERVO_DEFAULT_SPEED_DEG_PER_STEP 1
#define SERVO_DEFAULT_STEP_DELAY_MS       10

// === Wheels defaults ===
#define WHEEL_DEFAULT_SPEED 200   // 0..255 for quick tests
#define WHEEL_PWM_FREQ_HZ   10000 // 10 kHz
#define WHEEL_PWM_BITS      10    // 0..1023 duty

// === Serial ===
#define SERIAL_BAUD 115200
