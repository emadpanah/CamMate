#pragma once
#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
#include <vector>
#include "config.h"
#include "Speed.h"

struct RecFrame {
  uint32_t t;     // ms since start
  float x;        // -1..+1
  float y;        // -1..+1
  uint8_t mode;   // UIMode
  float diam;     // 0..1
  uint8_t speed;  // SpeedMode
};

enum RecState : uint8_t { REC_IDLE=0, REC_RECORDING=1, REC_PLAYING=2 };
enum PlayDir  : uint8_t { PLAY_FORWARD=0, PLAY_REVERSE=1 };

class Recorder {
public:
  bool begin();                         // mount SPIFFS
  void setSampleMs(uint16_t ms) { _sampleMs = ms; }

  // Recording (pathJson = .jsonl, pathMeta = .meta)
  bool startRecording(const char* pathJson="/rec.jsonl", const char* pathMeta="/rec.meta");
  bool stopRecording();

  // Playback (loads all frames into RAM)
  bool startPlayback(PlayDir dir, const char* pathJson="/rec.jsonl");
  void stopPlayback();

  // Call often from loop(): applies playback via callback
  void tick(uint32_t nowMs, void (*onApply)(const RecFrame&));

  RecState state() const { return _state; }
  const char* lastError() const { return _err; }

  // Utilities
  bool clearFile(const char* path);
  bool fileExists(const char* pathJson);
  static bool readMeta(const char* pathMeta, uint32_t& framesOut, uint32_t& durationMsOut);

  // Feed current joystick state while recording
  void pushLive(float x, float y, UIMode mode, float diam, SpeedMode spd);

private:
  bool _openWrite(const char* pathJson);
  void _closeWrite();
  bool _loadFile(const char* pathJson);

  RecState _state = REC_IDLE;
  File     _wf;
  char     _err[64] = {0};

  // record pacing
  uint16_t _sampleMs = 50; // ~20 Hz
  uint32_t _recStart = 0;
  uint32_t _nextSample = 0;

  // latest live values
  float   _lx=0, _ly=0, _ld=1.0f;
  UIMode  _lm = MODE_NORMAL;
  SpeedMode _ls = SPEED_NORMAL;

  // playback
  std::vector<RecFrame> _frames;
  PlayDir   _dir = PLAY_FORWARD;
  uint32_t  _playStart = 0;
  size_t    _idx = 0;

  // meta
  const char* _metaPath = "/rec.meta";
  uint32_t _framesRecorded = 0;
  uint32_t _lastT = 0;
};
