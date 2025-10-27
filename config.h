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

// === Servo ranges & LIMITS (HARD CLAMPS) ===
#define SERVO_MIN_DEG  0
#define SERVO_MAX_DEG  180
#define SERVO_CENTER   90

// Front (FF) must stay within 60..140
#define FF_MIN 60
#define FF_MAX 140
// Rear  (FR) must stay within 40..120
#define FR_MIN 40
#define FR_MAX 120

// === Motion tuning (servos) ===
#define SERVO_DEFAULT_SPEED_DEG_PER_STEP 1
#define SERVO_DEFAULT_STEP_DELAY_MS       10

// === Wheels defaults ===
#define WHEEL_DEFAULT_SPEED 200   // base speed mapping (we still clamp 0..255)
#define WHEEL_PWM_FREQ_HZ   10000 // 10 kHz
#define WHEEL_PWM_BITS      10    // 0..1023 duty

// === Speed caps (used by Low/Normal/Sport) ===
#define MAX_SPEED_NORMAL 140  // Normal & Low cap (0..255)
#define MAX_SPEED_SPORT  255  // Sport cap (0..255)
// Low = half of Normal result:
#define SPEED_LOW_FACTOR 0.5f

// === Wi-Fi AP ===
#define WIFI_SSID  "CamMate"
#define WIFI_PASS  "cammate123"

// >>> Custom AP IP/network
#define AP_IP_OCT1   192
#define AP_IP_OCT2   168
#define AP_IP_OCT3   12
#define AP_IP_OCT4   1

// Usually gateway = AP IP
#define AP_GW_OCT1   AP_IP_OCT1
#define AP_GW_OCT2   AP_IP_OCT2
#define AP_GW_OCT3   AP_IP_OCT3
#define AP_GW_OCT4   AP_IP_OCT4

// Subnet mask 255.255.255.0
#define AP_MASK_OCT1 255
#define AP_MASK_OCT2 255
#define AP_MASK_OCT3 255
#define AP_MASK_OCT4 0

// === HTTP server ===
#define HTTP_PORT 80

// === UI/Planner ===
enum UIMode : uint8_t { MODE_NORMAL=0, MODE_CRAB=1, MODE_CIRCLE=2 };
// scale speed down when steering is extreme (0=no scale, 1=max scale)
#define SPEED_STEER_SCALE 0.5f
