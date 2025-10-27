#pragma once
#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
#include <vector>
#include "config.h"
#include "Speed.h"

struct RecFrame {
  uint32_t t;     // ms since start
  uint8_t  manual;// 0=planner, 1=manual-steer
  float    x;     // -1..+1 (steer)
  float    y;     // -1..+1 (drive)
  uint8_t  mode;  // UIMode
  float    diam;  // 0..1
  uint8_t  speed; // SpeedMode
  int16_t  ff;    // front servo deg
  int16_t  fr;    // rear servo deg
};

enum RecState : uint8_t { REC_IDLE=0, REC_RECORDING=1, REC_PLAYING=2 };
enum PlayDir  : uint8_t { PLAY_FORWARD=0, PLAY_REVERSE=1 };

class Recorder {
public:
  bool begin();
  void setSampleMs(uint16_t ms) { _sampleMs = ms; }

  bool startRecording(const char* pathJson="/rec.jsonl", const char* pathMeta="/rec.meta");
  bool stopRecording();

  bool startPlayback(PlayDir dir, const char* pathJson="/rec.jsonl");
  void stopPlayback();

  void tick(uint32_t nowMs, void (*onApply)(const RecFrame&));

  RecState state() const { return _state; }
  const char* lastError() const { return _err; }

  bool clearFile(const char* path);
  bool fileExists(const char* pathJson);
  static bool readMeta(const char* pathMeta, uint32_t& framesOut, uint32_t& durationMsOut);

  void pushLive(bool manual, float x, float y, UIMode mode, float diam, SpeedMode spd, int ffDeg, int frDeg);

private:
  bool _openWrite(const char* pathJson);
  void _closeWrite();
  bool _loadFile(const char* pathJson);

  RecState _state = REC_IDLE;
  File     _wf;
  char     _err[64] = {0};

  uint16_t _sampleMs = 50;
  uint32_t _recStart = 0;
  uint32_t _nextSample = 0;

  // live data
  bool     _lmanual = true;
  float    _lx=0, _ly=0, _ld=1.0f;
  UIMode   _lm = MODE_NORMAL;
  SpeedMode _ls = SPEED_NORMAL;
  int16_t  _lff = SERVO_CENTER, _lfr = SERVO_CENTER;

  // playback
  std::vector<RecFrame> _frames;
  PlayDir   _dir = PLAY_FORWARD;
  uint32_t  _playStart = 0;
  size_t    _idx = 0;

  const char* _metaPath = "/rec.meta";
  uint32_t _framesRecorded = 0;
  uint32_t _lastT = 0;
};
