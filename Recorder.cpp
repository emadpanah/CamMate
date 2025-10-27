#include "Recorder.h"

static inline float clamp11f(float v){ if(v<-1)return-1; if(v>1)return 1; return v; }
static inline float clamp01f(float v){ if(v<0)return 0; if(v>1)return 1; return v; }

bool Recorder::begin(){
  if (!SPIFFS.begin(true)) { snprintf(_err,sizeof(_err),"SPIFFS mount fail"); return false; }
  return true;
}

bool Recorder::fileExists(const char* pathJson){ return SPIFFS.exists(pathJson); }
bool Recorder::clearFile(const char* path){ if (SPIFFS.exists(path)) return SPIFFS.remove(path); return true; }

bool Recorder::_openWrite(const char* pathJson){
  _wf = SPIFFS.open(pathJson, FILE_WRITE);
  if (!_wf) { snprintf(_err,sizeof(_err),"open write fail"); return false; }
  return true;
}
void Recorder::_closeWrite(){ if (_wf) _wf.close(); }

void Recorder::pushLive(bool manual, float x, float y, UIMode mode, float diam, SpeedMode spd, int ffDeg, int frDeg){
  _lmanual = manual;
  _lx = clamp11f(x); _ly = clamp11f(y);
  _lm = mode; _ld = clamp01f(diam); _ls = spd;
  _lff = (int16_t)ffDeg; _lfr = (int16_t)frDeg;
}

bool Recorder::startRecording(const char* pathJson, const char* pathMeta){
  if (_state != REC_IDLE) { snprintf(_err,sizeof(_err),"busy"); return false; }
  clearFile(pathJson); clearFile(pathMeta);
  if (!_openWrite(pathJson)) return false;
  _recStart   = millis();
  _nextSample = _recStart;
  _framesRecorded = 0;
  _lastT = 0;
  _metaPath = pathMeta;
  _state = REC_RECORDING;
  return true;
}

bool Recorder::stopRecording(){
  if (_state != REC_RECORDING) return false;
  _closeWrite();
  _state = REC_IDLE;
  File m = SPIFFS.open(_metaPath, FILE_WRITE);
  if (m) { m.printf("{\"frames\":%u,\"duration_ms\":%u}\n", _framesRecorded, _lastT); m.close(); }
  return true;
}

bool Recorder::_loadFile(const char* pathJson){
  _frames.clear();
  File f = SPIFFS.open(pathJson, FILE_READ);
  if (!f) { snprintf(_err,sizeof(_err),"open read fail"); return false; }
  String line;
  while (f.available()) {
    line = f.readStringUntil('\n'); line.trim();
    if (line.length() < 10) continue;
    RecFrame fr{};
    int ti=line.indexOf("\"t\":");
    int mai=line.indexOf("\"manual\":");
    int xi=line.indexOf("\"x\":");
    int yi=line.indexOf("\"y\":");
    int mi=line.indexOf("\"mode\":");
    int di=line.indexOf("\"diam\":");
    int si=line.indexOf("\"speed\":");
    int ffi=line.indexOf("\"ff\":");
    int fri=line.indexOf("\"fr\":");
    if (ti<0||xi<0||yi<0||mi<0||di<0||si<0) continue;

    fr.t     = (uint32_t)line.substring(ti+4).toInt();
    fr.manual= (uint8_t)((mai>=0)? line.substring(mai+9).toInt() : 0);
    fr.x     = line.substring(xi+4).toFloat();
    fr.y     = line.substring(yi+4).toFloat();
    fr.mode  = (uint8_t)line.substring(mi+7).toInt();
    fr.diam  = line.substring(di+7).toFloat();
    fr.speed = (uint8_t)line.substring(si+8).toInt();
    fr.ff    = (int16_t)((ffi>=0)? line.substring(ffi+5).toInt() : 90);
    fr.fr    = (int16_t)((fri>=0)? line.substring(fri+5).toInt() : 90);
    _frames.push_back(fr);
    if (_frames.size() > 30000) break;
  }
  f.close();
  if (_frames.empty()) { snprintf(_err,sizeof(_err),"no frames"); return false; }
  return true;
}

bool Recorder::startPlayback(PlayDir dir, const char* pathJson){
  if (_state != REC_IDLE) { snprintf(_err,sizeof(_err),"busy"); return false; }
  if (!fileExists(pathJson))  { snprintf(_err,sizeof(_err),"missing file"); return false; }
  if (!_loadFile(pathJson))   return false;
  _dir = dir;
  _playStart = millis();
  _idx = (dir==PLAY_FORWARD)?0:(_frames.size()-1);
  _state = REC_PLAYING;
  return true;
}

void Recorder::stopPlayback(){
  if (_state != REC_PLAYING) return;
  _state = REC_IDLE;
}

void Recorder::tick(uint32_t nowMs, void (*onApply)(const RecFrame&)){
  if (_state == REC_RECORDING) {
    if ((int32_t)(nowMs - _nextSample) >= 0) {
      uint32_t t = nowMs - _recStart;
      if (_wf) {
        _wf.printf("{\"t\":%u,\"manual\":%u,\"x\":%.3f,\"y\":%.3f,"
                   "\"mode\":%u,\"diam\":%.2f,\"speed\":%u,\"ff\":%d,\"fr\":%d}\n",
                   t,(unsigned)_lmanual,_lx,_ly,(unsigned)_lm,_ld,(unsigned)_ls,_lff,_lfr);
        _wf.flush();
        _framesRecorded++;
        _lastT = t;
      }
      _nextSample += _sampleMs;
    }
    return;
  }

  if (_state == REC_PLAYING) {
    if (_frames.empty()) { stopPlayback(); return; }
    uint32_t rel = nowMs - _playStart;
    if (_dir == PLAY_FORWARD) {
      while (_idx < _frames.size() && _frames[_idx].t <= rel) {
        onApply(_frames[_idx]);
        _idx++;
      }
      if (_idx >= _frames.size()) stopPlayback();
    } else {
      uint32_t total = _frames.back().t;
      uint32_t tback = (rel >= total) ? 0 : (total - rel);
      while (_idx < _frames.size() && _frames[_idx].t >= tback) {
        onApply(_frames[_idx]);
        if (_idx==0) { stopPlayback(); break; }
        _idx--;
      }
    }
  }
}

bool Recorder::readMeta(const char* pathMeta, uint32_t& framesOut, uint32_t& durationMsOut){
  framesOut = 0; durationMsOut = 0;
  if (!SPIFFS.exists(pathMeta)) return false;
  File f = SPIFFS.open(pathMeta, FILE_READ);
  if (!f) return false;
  String s = f.readString(); f.close();
  int fi = s.indexOf("\"frames\":");
  int di = s.indexOf("\"duration_ms\":");
  if (fi<0||di<0) return false;
  framesOut = (uint32_t)s.substring(fi+9).toInt();
  durationMsOut = (uint32_t)s.substring(di+14).toInt();
  return true;
}
