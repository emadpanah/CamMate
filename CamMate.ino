/*
  CamMate v3.9 – Solid UI + Dual Joysticks + 3-Speed + 5-Slot Recorder
  - Left joystick = Drive throttle (Y only)
  - Right joystick = Steering (Manual = direct servo, Off = planner)
  - Modes: Normal / Crab / Circle (diameter)
  - Speeds: Low / Normal / Sport
  - Recorder: 5 slots with auto-wrap, list/record/play/clear/abort
  - Fix: Record & replay servo angles when manual-steer is ON
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <FS.h>
#include <SPIFFS.h>

#include "config.h"
#include "ServoControl.h"
#include "WheelControl.h"
#include "Utils.h"
#include "MotionPlanner.h"
#include "Speed.h"
#include "Recorder.h"

#ifndef SERIAL_BAUD
#define SERIAL_BAUD 115200
#endif

// ==== Globals ====
ServoControl servoFront, servoRear;
WheelControl wheels;
WebServer server(HTTP_PORT);
Recorder recorder;

// State
volatile float driveY = 0.0f;
volatile float steerX = 0.0f;
volatile float steerY = 0.0f;
volatile UIMode uiMode = MODE_NORMAL;
volatile float circleDiam = 1.0f;     // 0..1
volatile bool manualSteer = true;     // ON = right pad moves servos directly
volatile bool eStop = false;

volatile int g_manualFF = SERVO_CENTER;  // last manual targets
volatile int g_manualFR = SERVO_CENTER;

// Helpers
static inline int clampInt(int v,int lo,int hi){return (v<lo)?lo:((v>hi)?hi:v);}
static inline float clamp11(float v){return (v<-1)?-1:((v>1)?1:v);}
static inline float clamp01(float v){return (v<0)?0:((v>1)?1:v);}
static inline void setFrontSteer(int d){ servoFront.writeDeg(clampInt(d, FF_MIN, FF_MAX)); }
static inline void setRearSteer (int d){ servoRear .writeDeg(clampInt(d, FR_MIN, FR_MAX)); }
static inline void centerSteer(){ setFrontSteer(SERVO_CENTER); setRearSteer(SERVO_CENTER); }

// ==== Recorder: 5-slot ring ====
static const int  REC_SLOTS = 5;
static const char* META_NEXT_PATH = "/rec_next.txt";

static int readNextSlot() {
  if (!SPIFFS.exists(META_NEXT_PATH)) return 1;
  File f = SPIFFS.open(META_NEXT_PATH, FILE_READ);
  if (!f) return 1;
  String s = f.readString(); f.close();
  int n = s.toInt();
  if (n < 1 || n > REC_SLOTS) n = 1;
  return n;
}
static void writeNextSlot(int next) {
  if (next < 1 || next > REC_SLOTS) next = 1;
  File f = SPIFFS.open(META_NEXT_PATH, FILE_WRITE);
  if (!f) return;
  f.printf("%d", next);
  f.close();
}
static void pathForSlot(int slot, char* dst, size_t cap, bool meta=false) {
  snprintf(dst, cap, meta ? "/rec%d.meta" : "/rec%d.jsonl", slot);
}
static int slotAfter(int slot) { ++slot; if (slot > REC_SLOTS) slot = 1; return slot; }

// ==== UI (ASCII only; flexible layout) ====
const char PAGE_INDEX[] PROGMEM = R"HTML(
<!doctype html><html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>CamMate</title>
<style>
  :root{ --pad: min(40vw, 360px); --dot: 18px; }
  body{font-family:sans-serif;margin:10px}
  .row{display:flex;gap:12px;flex-wrap:wrap;align-items:flex-start}
  .card{border:1px solid #ddd;border-radius:12px;padding:12px}
  .pads{display:flex;gap:12px;flex-wrap:wrap}
  .pad{width:var(--pad);height:var(--pad);background:#f2f2f2;border-radius:12px;position:relative;touch-action:none}
  .pad .label{position:absolute;left:10px;top:10px;background:#fff;border:1px solid #ccc;border-radius:8px;padding:2px 8px;font-size:12px}
  .dot{position:absolute;width:var(--dot);height:var(--dot);border-radius:50%;background:#222;transform:translate(-50%,-50%);border:2px solid #fff;box-shadow:0 0 3px rgba(0,0,0,0.4)}
  h2{margin:6px 0 12px}
  button{padding:8px 12px;border-radius:8px;border:1px solid #aaa;background:#fafafa;margin:3px;cursor:pointer}
  button.sel{background:#333;color:#fff}
  button.warn{background:#ffecec;border-color:#e53935}
  .badge{display:inline-block;padding:2px 8px;border-radius:999px;background:#222;color:#fff;font-size:12px}
  input[type=range]{width:220px}
  table{border-collapse:collapse;margin-top:8px;font-size:13px}
  th,td{padding:6px 8px;border:1px solid #ddd}
  .stack{display:flex;flex-direction:column;gap:10px}
</style>
</head><body>
<h2>CamMate <span id="spdBadge" class="badge">Normal</span></h2>

<div class="row">
  <div class="pads">
    <div class="pad card" id="padDrive">
      <div class="label">Drive</div>
      <div class="dot" id="dotDrive" style="left:50%;top:50%"></div>
    </div>
    <div class="pad card" id="padSteer">
      <div class="label">Steer</div>
      <div class="dot" id="dotSteer" style="left:50%;top:50%"></div>
    </div>
  </div>

  <div class="card stack">
    <div>
      <div>Manual Steer:
        <button id="msOn"  class="sel">On</button>
        <button id="msOff">Off</button>
      </div>
      <div id="modeRow" style="margin-top:6px">
        Mode:
        <label><input type="radio" name="mode" value="0" checked> Normal</label>
        <label><input type="radio" name="mode" value="1"> Crab</label>
        <label><input type="radio" name="mode" value="2"> Circle</label>
      </div>
      <div id="diamRow" style="margin-top:6px">
        Circle diameter
        <input id="diam" type="range" min="0" max="1" step="0.01" value="1">
        <span id="dval" class="badge">1.00</span>
      </div>
    </div>

    <div>
      <button id="center">Center</button>
      <button id="stop" class="warn">STOP</button>
      <button id="abort" class="warn">Stop Playback</button>
    </div>

    <div>
      Speed:
      <button id="spdLow">Low</button>
      <button id="spdNormal" class="sel">Normal</button>
      <button id="spdSport">Sport</button>
    </div>
  </div>

  <div class="card" style="min-width:320px">
    <h3 style="margin:4px 0 8px">Recorder (5 slots)</h3>
    <div>
      Active slot:
      <span id="slots"></span>
      <button id="refresh">Refresh</button>
    </div>
    <table id="tbl">
      <thead><tr><th>#</th><th>Status</th><th>Frames</th><th>Dur(ms)</th><th>Size</th></tr></thead>
      <tbody></tbody>
    </table>
    <div style="margin-top:8px">
      <button id="recAuto"  class="warn">Record (Auto-slot)</button>
      <button id="recTo">Record to Slot</button>
      <button id="recStop">Stop Record</button>
    </div>
    <div style="margin-top:6px">
      <button id="playF">Play Forward</button>
      <button id="playR">Play Reverse</button>
      <button id="clear">Clear Slot</button>
    </div>
  </div>
</div>

<script>
const DEAD=0.07; // dead-zone
let manual=true, mode=0, diam=1.0, slot=1;
let spd='normal';
const badge=document.getElementById('spdBadge');

function clamp(v,a,b){return v<a?a:(v>b?b:v);}
function dead(v){return Math.abs(v)<DEAD?0:v;}

function setupPad(id, cb){
  const pad=document.getElementById(id), dot=pad.querySelector('.dot');
  let active=false;
  function setDot(px,py){ dot.style.left=px+'px'; dot.style.top=py+'px'; }
  function center(){ setDot(pad.clientWidth/2, pad.clientHeight/2); }
  pad.addEventListener('pointerdown',e=>{active=true;pad.setPointerCapture(e.pointerId);handle(e);});
  pad.addEventListener('pointermove',e=>{ if(active) handle(e); });
  pad.addEventListener('pointerup',e=>{ active=false; center(); cb(0,0); });
  function handle(e){
    const r=pad.getBoundingClientRect();
    let px=e.clientX, py=e.clientY;
    px=clamp(px,r.left,r.right); py=clamp(py,r.top,r.bottom);
    const nx=((px-r.left)/r.width)*2-1;
    const ny=((py-r.top)/r.height)*2-1;
    const x=dead(nx), y=dead(ny);
    setDot(px-r.left,py-r.top);
    cb(x,-y);
  }
  center();
}

setupPad('padDrive', (x,y)=>{
  fetch(`/ctl_drive?y=${y.toFixed(3)}`).catch(()=>{});
});

setupPad('padSteer', (x,y)=>{
  if (manual)
    fetch(`/ctl_servos?x=${x.toFixed(3)}&y=${y.toFixed(3)}`).catch(()=>{});
  else
    fetch(`/ctl_steer?x=${x.toFixed(3)}&y=${y.toFixed(3)}&mode=${mode}&diam=${diam.toFixed(2)}`).catch(()=>{});
});

// Manual steer toggle
const msOn = document.getElementById('msOn');
const msOff= document.getElementById('msOff');
function setManual(on){
  manual=on;
  msOn.classList.toggle('sel', on);
  msOff.classList.toggle('sel', !on);
  document.getElementById('modeRow').style.opacity = on ? 0.45 : 1;
  document.getElementById('diamRow').style.opacity = on ? 0.45 : 1;
  fetch(`/ui/manual_steer?on=${on?1:0}`).catch(()=>{});
}
msOn.onclick = ()=> setManual(true);
msOff.onclick= ()=> setManual(false);

// Modes + diameter
document.querySelectorAll('input[name=mode]').forEach(r=>{
  r.addEventListener('change', ()=>{
    mode = Number(document.querySelector('input[name=mode]:checked').value);
  });
});
const diamEl=document.getElementById('diam'), dval=document.getElementById('dval');
diamEl.addEventListener('input', ()=>{
  diam = Number(diamEl.value);
  dval.textContent = diam.toFixed(2);
});

// Speed
function setSpeed(s){
  spd=s;
  badge.textContent = s.charAt(0).toUpperCase()+s.slice(1);
  document.getElementById('spdLow').classList.toggle('sel', s==='low');
  document.getElementById('spdNormal').classList.toggle('sel', s==='normal');
  document.getElementById('spdSport').classList.toggle('sel', s==='sport');
  fetch(`/speed?mode=${s}`).catch(()=>{});
}
document.getElementById('spdLow').onclick=()=>setSpeed('low');
document.getElementById('spdNormal').onclick=()=>setSpeed('normal');
document.getElementById('spdSport').onclick=()=>setSpeed('sport');

// Center / stop / abort
document.getElementById('center').onclick=()=>fetch('/center').catch(()=>{});
document.getElementById('stop').onclick  =()=>fetch('/stop').catch(()=>{});
document.getElementById('abort').onclick =()=>fetch('/rec/abort').catch(()=>{});

// Recorder UI
const slotsDiv=document.getElementById('slots');
const tbody=document.querySelector('#tbl tbody');
function drawSlots(){
  slotsDiv.innerHTML='';
  for(let i=1;i<=5;i++){
    const b=document.createElement('button');
    b.textContent=i; if(i===slot) b.classList.add('sel');
    b.onclick=()=>{ slot=i; drawSlots(); };
    slotsDiv.appendChild(b);
  }
}
drawSlots();

function refreshList(){
  fetch('/rec/list').then(r=>r.json()).then(arr=>{
    tbody.innerHTML='';
    arr.forEach(it=>{
      const tr=document.createElement('tr');
      tr.innerHTML = `<td>${it.slot}</td>
        <td>${it.exists?'OK':'(empty)'}</td>
        <td>${it.frames||0}</td>
        <td>${it.duration_ms||0}</td>
        <td>${it.bytes||0}</td>`;
      tbody.appendChild(tr);
    });
  }).catch(()=>{});
}
document.getElementById('refresh').onclick=refreshList;
refreshList();

// Record / Play / Clear
document.getElementById('recAuto').onclick = ()=>fetch('/rec/start').then(refreshList);
document.getElementById('recTo').onclick   = ()=>fetch(`/rec/start?slot=${slot}`).then(refreshList);
document.getElementById('recStop').onclick = ()=>fetch('/rec/stop').then(refreshList);
document.getElementById('playF').onclick   = ()=>fetch(`/rec/play?slot=${slot}&dir=f`);
document.getElementById('playR').onclick   = ()=>fetch(`/rec/play?slot=${slot}&dir=r`);
document.getElementById('clear').onclick   = ()=>fetch(`/rec/clear?slot=${slot}`).then(refreshList);

// Defaults
setManual(true);
setSpeed('normal');
</script>
</body></html>
)HTML";

// ==== HTTP Handlers ====
static void handleIndex(){ server.send_P(200, "text/html", PAGE_INDEX); }

static void handleCtlDrive(){
  if (server.hasArg("y")) driveY = clamp11(server.arg("y").toFloat());
  recorder.pushLive(manualSteer, steerX, driveY, uiMode, circleDiam, g_speedMode, g_manualFF, g_manualFR);
  eStop = false;
  server.send(204);
}

static void handleCtlSteer(){
  if (server.hasArg("x")) steerX = clamp11(server.arg("x").toFloat());
  if (server.hasArg("y")) steerY = clamp11(server.arg("y").toFloat());
  if (server.hasArg("mode")) uiMode = (UIMode)server.arg("mode").toInt();
  if (server.hasArg("diam")) circleDiam = clamp01(server.arg("diam").toFloat());
  recorder.pushLive(manualSteer, steerX, driveY, uiMode, circleDiam, g_speedMode, g_manualFF, g_manualFR);
  eStop = false;
  server.send(204);
}

static void handleCtlServos(){
  float x = 0, y = 0;
  if (server.hasArg("x")) x = clamp11(server.arg("x").toFloat());
  if (server.hasArg("y")) y = clamp11(server.arg("y").toFloat());
  int ff = SERVO_CENTER + (int)roundf(x * (FF_MAX - SERVO_CENTER));
  int fr = SERVO_CENTER + (int)roundf(y * (FR_MAX - SERVO_CENTER));
  g_manualFF = clampInt(ff, FF_MIN, FF_MAX);
  g_manualFR = clampInt(fr, FR_MIN, FR_MAX);
  recorder.pushLive(true, steerX, driveY, uiMode, circleDiam, g_speedMode, g_manualFF, g_manualFR);
  eStop = false;
  server.send(204);
}

static void handleManual(){ // /ui/manual_steer?on=1|0
  if (server.hasArg("on")) manualSteer = (server.arg("on").toInt()!=0);
  server.send(204);
}

static void handleCenter(){ centerSteer(); eStop=false; server.send(204); }
static void handleStop(){ eStop = true; server.send(204); }

static void handleSpeed(){
  if (server.hasArg("mode")) {
    String m = server.arg("mode");
    if      (m=="sport")  g_speedMode = SPEED_SPORT;
    else if (m=="low")    g_speedMode = SPEED_LOW;
    else                  g_speedMode = SPEED_NORMAL;
  }
  server.send(204);
}

// Recorder: list/record/play/clear/abort (slot-aware)
static void handleRecList(){
  String out = "[";
  for (int s=1; s<=REC_SLOTS; ++s) {
    char pJson[20], pMeta[20];
    pathForSlot(s, pJson, sizeof(pJson), false);
    pathForSlot(s, pMeta, sizeof(pMeta), true);
    bool exists = SPIFFS.exists(pJson);
    uint32_t frames=0, dur=0, bytes=0;
    if (exists) {
      File f = SPIFFS.open(pJson, FILE_READ);
      if (f) { bytes = f.size(); f.close(); }
      Recorder::readMeta(pMeta, frames, dur);
    }
    out += String("{\"slot\":")+s+",\"exists\":"+(exists?"true":"false")+
           ",\"frames\":"+frames+",\"duration_ms\":"+dur+",\"bytes\":"+bytes+"}";
    if (s<REC_SLOTS) out += ",";
  }
  out += "]";
  server.send(200,"application/json", out);
}

static void handleRecStart(){
  int reqSlot = server.hasArg("slot") ? server.arg("slot").toInt() : 0;
  int slot = (reqSlot>=1 && reqSlot<=REC_SLOTS) ? reqSlot : readNextSlot();
  char pJson[20], pMeta[20];
  pathForSlot(slot, pJson, sizeof(pJson), false);
  pathForSlot(slot, pMeta, sizeof(pMeta), true);
  if (recorder.startRecording(pJson, pMeta)) {
    if (reqSlot==0) writeNextSlot(slotAfter(slot));
    server.send(200,"text/plain","REC START");
  } else {
    server.send(500,"text/plain", recorder.lastError());
  }
}
static void handleRecStop(){ recorder.stopRecording(); server.send(200,"text/plain","REC STOP"); }

static void handleRecPlay(){
  int slot = server.hasArg("slot") ? server.arg("slot").toInt() : 1;
  char pJson[20]; pathForSlot(slot, pJson, sizeof(pJson), false);
  String dir = server.hasArg("dir") ? server.arg("dir") : "f";
  bool ok = recorder.startPlayback(dir=="r" ? PLAY_REVERSE : PLAY_FORWARD, pJson);
  if (ok) server.send(200,"text/plain","PLAY");
  else    server.send(500,"text/plain", recorder.lastError());
}
static void handleRecClear(){
  int slot = server.hasArg("slot") ? server.arg("slot").toInt() : 1;
  char pJson[20], pMeta[20];
  pathForSlot(slot, pJson, sizeof(pJson), false);
  pathForSlot(slot, pMeta, sizeof(pMeta), true);
  bool ok1 = recorder.clearFile(pJson);
  bool ok2 = recorder.clearFile(pMeta);
  server.send((ok1&&ok2)?200:500, "text/plain", (ok1&&ok2)?"CLEARED":"ERR");
}
static void handleRecAbort(){ recorder.stopPlayback(); eStop = true; server.send(200,"text/plain","ABORTED"); }

// ==== WiFi + HTTP ====
static void initWiFi(){
  WiFi.mode(WIFI_AP);
  IPAddress apIP(AP_IP_OCT1,AP_IP_OCT2,AP_IP_OCT3,AP_IP_OCT4);
  IPAddress gw(AP_GW_OCT1,AP_GW_OCT2,AP_GW_OCT3,AP_GW_OCT4);
  IPAddress msk(AP_MASK_OCT1,AP_MASK_OCT2,AP_MASK_OCT3,AP_MASK_OCT4);
  bool cfg = WiFi.softAPConfig(apIP,gw,msk);
  bool ok  = WiFi.softAP(WIFI_SSID, WIFI_PASS);
  delay(200);
  Serial.printf("[WiFi] %s  SSID:%s  IP:%s  (cfg:%s)\n",
    ok?"AP STARTED":"FAILED", WIFI_SSID, WiFi.softAPIP().toString().c_str(), cfg?"OK":"ERR");
}
static void initHttp(){
  server.on("/", HTTP_GET, handleIndex);

  server.on("/ctl_drive",  HTTP_GET, handleCtlDrive);
  server.on("/ctl_steer",  HTTP_GET, handleCtlSteer);
  server.on("/ctl_servos", HTTP_GET, handleCtlServos);
  server.on("/ui/manual_steer", HTTP_GET, handleManual);

  server.on("/center", HTTP_GET, handleCenter);
  server.on("/stop",   HTTP_GET, handleStop);
  server.on("/speed",  HTTP_GET, handleSpeed);

  server.on("/rec/list",  HTTP_GET, handleRecList);
  server.on("/rec/start", HTTP_GET, handleRecStart);
  server.on("/rec/stop",  HTTP_GET, handleRecStop);
  server.on("/rec/play",  HTTP_GET, handleRecPlay);
  server.on("/rec/clear", HTTP_GET, handleRecClear);
  server.on("/rec/abort", HTTP_GET, handleRecAbort);

  server.begin();
  Serial.printf("[HTTP] Listening on %d\n", HTTP_PORT);
}

// ==== Setup & Loop ====
void setup(){
  Serial.begin(SERIAL_BAUD); delay(400);
  Serial.println(F("\n=== CamMate v3.9 ==="));

  SPIFFS.begin(true);

  servoFront.attach(SERVO_FRONT_PIN);
  servoRear.attach(SERVO_REAR_PIN);
  centerSteer();

  WheelPins p{ L298_IN1, L298_IN2, L298_ENA, L298_IN3, L298_IN4, L298_ENB };
  wheels.begin(p, WHEEL_PWM_FREQ_HZ, WHEEL_PWM_BITS);

  if (recorder.begin()) recorder.setSampleMs(50); // 20Hz
  else Serial.println(F("[REC] SPIFFS mount failed"));

  initWiFi();
  initHttp();

  IPAddress ip = WiFi.softAPIP();
  Serial.printf("[UI] Connect '%s' / '%s' -> http://%s/\n", WIFI_SSID, WIFI_PASS, ip.toString().c_str());
}

void loop(){
  // Apply recorder playback state (incl. servo angles when manual)
  recorder.tick(millis(), [](const RecFrame& fr){
    manualSteer = (fr.manual != 0);
    steerX      = fr.x;
    driveY      = fr.y;
    uiMode      = (UIMode)fr.mode;
    circleDiam  = fr.diam;
    g_speedMode = (SpeedMode)fr.speed;
    if (manualSteer) {
      g_manualFF = clampInt(fr.ff, FF_MIN, FF_MAX);
      g_manualFR = clampInt(fr.fr, FR_MIN, FR_MAX);
    }
  });

  server.handleClient();

  // Compute & apply motion
  int ff=SERVO_CENTER, fr=SERVO_CENTER;
  int base = (int)(driveY * 255.0f);
  float steerExtent = 0.0f;

  if (!manualSteer) {
    planSteering(steerX, driveY, uiMode, circleDiam, ff, fr, base, steerExtent);
    setFrontSteer(ff);
    setRearSteer(fr);
  } else {
    setFrontSteer(g_manualFF);
    setRearSteer(g_manualFR);
  }

  int pwm = applySpeedScaling(base, steerExtent);
  wheels.setSpeedBoth(eStop ? 0 : clampInt(pwm, -255, 255));
}
