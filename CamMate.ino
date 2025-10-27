/*
  CamMate v3.5 – Wi-Fi AP + Joystick + Low/Normal/Sport + Recorder (5-slot ring)
  - 5 recording slots: /rec1.jsonl ... /rec5.jsonl  (+ .meta with frames & duration)
  - Auto-record wraps: the 6th overwrites slot #1, etc.
  - Web UI: pick slot, record (auto or to slot), play, reverse, stop, clear, list
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

// -------- Globals --------
ServoControl servoFront, servoRear;
WheelControl wheels;
WebServer server(HTTP_PORT);
Recorder recorder;

volatile float  joyX = 0.0f, joyY = 0.0f;
volatile UIMode uiMode = MODE_NORMAL;
volatile float  circleDiam = 1.0f;
volatile bool   eStop = false;

// helpers
static inline void setFrontSteer(int d){ servoFront.writeDeg(constrain(d, FF_MIN, FF_MAX)); }
static inline void setRearSteer (int d){ servoRear .writeDeg(constrain(d, FR_MIN, FR_MAX)); }
static inline void centerSteer(){ setFrontSteer(SERVO_CENTER); setRearSteer(SERVO_CENTER); }

// -------- Recorder ring (5 slots) helpers --------
static const int  REC_SLOTS = 5;
static const char* META_NEXT_PATH = "/rec_next.txt";

static int readNextSlot() {
  if (!SPIFFS.exists(META_NEXT_PATH)) return 1;
  File f = SPIFFS.open(META_NEXT_PATH, FILE_READ);
  if (!f) return 1;
  String s = f.readString();
  f.close();
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
static int slotAfter(int slot) {
  ++slot; if (slot > REC_SLOTS) slot = 1; return slot;
}

// -------- Web UI (HTML) --------
const char PAGE_INDEX[] PROGMEM = R"HTML(
<!doctype html><html><head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>CamMate</title>
<style>
body{font-family:sans-serif;margin:12px}
.row{display:flex;gap:12px;flex-wrap:wrap}
#pad{width:260px;height:260px;background:#eee;border-radius:12px;position:relative;touch-action:none}
#dot{position:absolute;width:14px;height:14px;border-radius:50%;background:#333;transform:translate(-50%,-50%)}
.card{padding:12px;border:1px solid #ddd;border-radius:12px}
button{padding:10px 14px;border-radius:10px;border:1px solid #999;background:#fafafa;margin-right:4px;cursor:pointer}
button.sel{background:#444;color:#fff}
button.warn{background:#fbe9e7;border-color:#f4511e}
.badge{display:inline-block;padding:2px 8px;border-radius:999px;background:#222;color:#fff;font-size:12px}
input[type=range]{width:220px}
table{border-collapse:collapse;margin-top:6px}
td,th{padding:6px 10px;border:1px solid #ddd;font-size:13px}
small code{background:#f4f4f4;padding:2px 6px;border-radius:6px}
</style></head><body>
<h2>CamMate Controller <span id="speedBadge" class="badge">Normal</span></h2>

<div class="row">
  <!-- Joystick -->
  <div id="pad" class="card"><div id="dot" style="left:130px;top:130px"></div></div>

  <!-- Controls -->
  <div class="card">
    <div>Drive Mode:
      <label><input type="radio" name="mode" value="0" checked> Normal</label>
      <label><input type="radio" name="mode" value="1"> Crab</label>
      <label><input type="radio" name="mode" value="2"> Circle</label>
    </div>

    <div style="margin-top:10px">Circle diameter
      <input id="diam" type="range" min="0" max="1" step="0.01" value="1">
      <span id="dval" class="badge">1.00</span>
    </div>

    <div style="margin-top:10px">
      <button id="center">Center</button>
      <button id="stop">STOP</button>
    </div>

    <div style="margin-top:10px">
      Speed:
      <button id="spdLow">Low</button>
      <button id="spdNormal" class="sel">Normal</button>
      <button id="spdSport">Sport</button>
    </div>
  </div>

  <!-- Recorder -->
  <div class="card">
    <div style="margin-bottom:6px"><b>Recorder (5 slots, auto-wrap)</b></div>
    <div>
      Slots:
      <span id="slotBtns"></span>
      <button id="refresh">↻</button>
    </div>
    <table id="slotTbl">
      <thead><tr><th>#</th><th>Status</th><th>Frames</th><th>Duration</th><th>Size</th></tr></thead>
      <tbody></tbody>
    </table>
    <div style="margin-top:8px">
      <button id="recAuto"  class="warn">Record (Auto)</button>
      <button id="recTo">Record to Slot</button>
      <button id="recStop">Stop</button>
    </div>
    <div style="margin-top:6px">
      <button id="playF">Play ▶︎</button>
      <button id="playR">Play ◀︎</button>
      <button id="clear">Clear Slot</button>
    </div>
    <small style="display:block;margin-top:8px">
      Auto mode writes to the next slot and wraps (6th replaces #1).<br>
      Serial: <code>recstart [slot]</code>, <code>recstop</code>, <code>play [slot]</code>, <code>playr [slot]</code>, <code>recclear [slot]</code>.
    </small>
  </div>
</div>

<script>
const pad=document.getElementById('pad'),dot=document.getElementById('dot'),
diam=document.getElementById('diam'),dval=document.getElementById('dval'),
tbl=document.querySelector('#slotTbl tbody'), slotBtnsSpan=document.getElementById('slotBtns'),
badge=document.getElementById('speedBadge');

let mode=0,x=0,y=0,active=false,lastSend=0,spd='normal',slot=1;

function setDot(px,py){dot.style.left=px+'px';dot.style.top=py+'px';}
function send(){
  const now=Date.now(); if(now-lastSend<60)return; lastSend=now;
  fetch(`/ctl?x=${x.toFixed(3)}&y=${y.toFixed(3)}&mode=${mode}&diam=${Number(diam.value).toFixed(2)}`).catch(()=>{});
}
function handle(e){
  const r=pad.getBoundingClientRect(),cx=r.left+r.width/2,cy=r.top+r.height/2;
  let px=e.touches?e.touches[0].clientX:e.clientX,py=e.touches?e.touches[0].clientY:e.clientY;
  px=Math.max(r.left,Math.min(r.right,px)); py=Math.max(r.top,Math.min(r.bottom,py));
  const nx=(px-cx)/(r.width/2), ny=(cy-py)/(r.height/2);
  x=Math.max(-1,Math.min(1,nx)); y=Math.max(-1,Math.min(1,ny));
  setDot(px-r.left,py-r.top); send();
}
pad.addEventListener('pointerdown',e=>{active=true;pad.setPointerCapture(e.pointerId);handle(e);});
pad.addEventListener('pointermove',e=>{if(active)handle(e);});
pad.addEventListener('pointerup',e=>{active=false;x=0;y=0;setDot(pad.clientWidth/2,pad.clientHeight/2);send();});
document.querySelectorAll('input[name=mode]').forEach(r=>r.addEventListener('change',()=>{mode=Number(document.querySelector('input[name=mode]:checked').value);send();}));
diam.addEventListener('input',()=>{dval.textContent=Number(diam.value).toFixed(2);send();});
document.getElementById('center').onclick=()=>{fetch('/center').catch(()=>{});};
document.getElementById('stop').onclick=()=>{fetch('/stop').catch(()=>{});};

function setSpeed(newSpd){
  spd=newSpd;
  document.getElementById('spdLow').classList.remove('sel');
  document.getElementById('spdNormal').classList.remove('sel');
  document.getElementById('spdSport').classList.remove('sel');
  if(spd==='low') document.getElementById('spdLow').classList.add('sel');
  if(spd==='normal') document.getElementById('spdNormal').classList.add('sel');
  if(spd==='sport') document.getElementById('spdSport').classList.add('sel');
  badge.textContent = spd.charAt(0).toUpperCase()+spd.slice(1);
  fetch(`/speed?mode=${spd}`).catch(()=>{});
}
document.getElementById('spdLow').onclick   = ()=>setSpeed('low');
document.getElementById('spdNormal').onclick= ()=>setSpeed('normal');
document.getElementById('spdSport').onclick = ()=>setSpeed('sport');

// ---- Recorder UI ----
function renderSlots(list){
  // buttons
  slotBtnsSpan.innerHTML='';
  list.forEach(it=>{
    const b=document.createElement('button');
    b.textContent=it.slot;
    b.className = (slot===it.slot)?'sel':'';
    b.onclick=()=>{slot=it.slot; renderSlots(list);}
    slotBtnsSpan.appendChild(b);
  });
  // table
  tbl.innerHTML='';
  list.forEach(it=>{
    const tr=document.createElement('tr');
    tr.innerHTML = `<td>${it.slot}</td>
      <td>${it.exists?'Saved':'—'}</td>
      <td>${it.frames||0}</td>
      <td>${it.duration_ms? (it.duration_ms/1000).toFixed(1)+'s' : '—'}</td>
      <td>${it.bytes? it.bytes+' B' : '—'}</td>`;
    tbl.appendChild(tr);
  });
}
function refreshList(){ fetch('/rec/list').then(r=>r.json()).then(renderSlots).catch(()=>{}); }
document.getElementById('refresh').onclick=refreshList;

document.getElementById('recAuto').onclick = ()=>{ fetch('/rec/start').then(()=>refreshList()); };
document.getElementById('recTo').onclick   = ()=>{ fetch('/rec/start?slot='+slot).then(()=>refreshList()); };
document.getElementById('recStop').onclick = ()=>{ fetch('/rec/stop').then(()=>refreshList()); };

document.getElementById('playF').onclick = ()=>{ fetch('/rec/play?dir=f&slot='+slot).catch(()=>{}); };
document.getElementById('playR').onclick = ()=>{ fetch('/rec/play?dir=r&slot='+slot).catch(()=>{}); };
document.getElementById('clear').onclick = ()=>{ fetch('/rec/clear?slot='+slot).then(()=>refreshList()); };

refreshList(); // initial
</script>
</body></html>
)HTML";

// -------- HTTP handlers --------
static void handleIndex(){ server.send_P(200,"text/html",PAGE_INDEX); }

static void handleCtl(){
  if (server.hasArg("x"))    joyX       = server.arg("x").toFloat();
  if (server.hasArg("y"))    joyY       = server.arg("y").toFloat();
  if (server.hasArg("mode")) uiMode     = (UIMode)server.arg("mode").toInt();
  if (server.hasArg("diam")) circleDiam = server.arg("diam").toFloat();
  recorder.pushLive(joyX, joyY, uiMode, circleDiam, g_speedMode);
  eStop = false;
  server.send(204);
}
static void handleStop(){ eStop = true; server.send(204); }
static void handleCenter(){ centerSteer(); eStop = false; server.send(204); }
static void handleSpeed(){
  if (server.hasArg("mode")) {
    String m = server.arg("mode");
    if      (m=="sport")  g_speedMode = SPEED_SPORT;
    else if (m=="low")    g_speedMode = SPEED_LOW;
    else                  g_speedMode = SPEED_NORMAL;
    Serial.printf("[SPEED] Mode: %s\n", g_speedMode==SPEED_SPORT?"SPORT":(g_speedMode==SPEED_LOW?"LOW":"NORMAL"));
  }
  server.send(204);
}

// Build JSON list of 5 slots with exists/frames/duration/bytes
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

// Start recording: optional ?slot=N ; if missing -> auto with wrap
static void handleRecStart(){
  int reqSlot = server.hasArg("slot") ? server.arg("slot").toInt() : 0;
  int slot = reqSlot>=1 && reqSlot<=REC_SLOTS ? reqSlot : readNextSlot();

  char pJson[20], pMeta[20];
  pathForSlot(slot, pJson, sizeof(pJson), false);
  pathForSlot(slot, pMeta, sizeof(pMeta), true);

  // start recording to this slot
  if (recorder.startRecording(pJson, pMeta)) {
    // if auto, advance pointer for NEXT time
    if (reqSlot==0) writeNextSlot(slotAfter(slot));
    Serial.printf("[REC] start slot #%d -> %s\n", slot, pJson);
    server.send(200,"text/plain","REC START");
  } else {
    server.send(500,"text/plain", recorder.lastError());
  }
}
static void handleRecStop(){ recorder.stopRecording(); server.send(200,"text/plain","REC STOP"); }

// Play: ?slot=N&dir=f|r
static void handleRecPlay(){
  int slot = server.hasArg("slot") ? server.arg("slot").toInt() : 1;
  char pJson[20]; pathForSlot(slot, pJson, sizeof(pJson), false);
  String dir = server.hasArg("dir") ? server.arg("dir") : "f";
  bool ok = recorder.startPlayback(dir=="r" ? PLAY_REVERSE : PLAY_FORWARD, pJson);
  if (ok) server.send(200,"text/plain","PLAY");
  else    server.send(500,"text/plain", recorder.lastError());
}

// Clear: ?slot=N   (removes jsonl + meta)
static void handleRecClear(){
  int slot = server.hasArg("slot") ? server.arg("slot").toInt() : 1;
  char pJson[20], pMeta[20];
  pathForSlot(slot, pJson, sizeof(pJson), false);
  pathForSlot(slot, pMeta, sizeof(pMeta), true);
  bool ok1 = recorder.clearFile(pJson);
  bool ok2 = recorder.clearFile(pMeta);
  server.send((ok1&&ok2)?200:500, "text/plain", (ok1&&ok2)?"CLEARED":"ERR");
}

// -------- Init WiFi & HTTP --------
static void initWiFi(){
  WiFi.mode(WIFI_AP);
  IPAddress apIP(AP_IP_OCT1,AP_IP_OCT2,AP_IP_OCT3,AP_IP_OCT4);
  IPAddress gw(AP_GW_OCT1,AP_GW_OCT2,AP_GW_OCT3,AP_GW_OCT4);
  IPAddress msk(AP_MASK_OCT1,AP_MASK_OCT2,AP_MASK_OCT3,AP_MASK_OCT4);
  bool cfg=WiFi.softAPConfig(apIP,gw,msk);
  bool ok=WiFi.softAP(WIFI_SSID,WIFI_PASS);
  delay(200);
  Serial.printf("[WiFi] %s  SSID:%s  IP:%s  (cfg:%s)\n", ok?"AP STARTED":"FAILED",
                WIFI_SSID, WiFi.softAPIP().toString().c_str(), cfg?"OK":"ERR");
}
static void initHttp(){
  server.on("/",        HTTP_GET, handleIndex);
  server.on("/ctl",     HTTP_GET, handleCtl);
  server.on("/stop",    HTTP_GET, handleStop);
  server.on("/center",  HTTP_GET, handleCenter);
  server.on("/speed",   HTTP_GET, handleSpeed);

  // Recorder endpoints
  server.on("/rec/list",  HTTP_GET, handleRecList);
  server.on("/rec/start", HTTP_GET, handleRecStart);
  server.on("/rec/stop",  HTTP_GET, handleRecStop);
  server.on("/rec/play",  HTTP_GET, handleRecPlay);
  server.on("/rec/clear", HTTP_GET, handleRecClear);

  server.begin();
  Serial.printf("[HTTP] Web server on port %d\n", HTTP_PORT);
}

// -------- Legacy serial helpers --------
enum SelectedServo { SEL_FRONT = 0, SEL_REAR = 1 };
SelectedServo selected = SEL_FRONT;

void selectFront(){ selected = SEL_FRONT; Serial.println(F("[SELECT] Front servo selected")); }
void selectRear(){  selected = SEL_REAR;  Serial.println(F("[SELECT] Rear  servo selected"));  }

void moveSelectedBy(int delta){
  if (selected == SEL_FRONT) {
    int cur = servoFront.readDeg(); if (cur<0) cur = SERVO_CENTER;
    int nxt = constrain(cur + delta, FF_MIN, FF_MAX);
    servoFront.writeDeg(nxt);
    Serial.printf("[MOVE] Front: %d -> %d (delta %+d)\n", cur, nxt, delta);
  } else {
    int cur = servoRear.readDeg(); if (cur<0) cur = SERVO_CENTER;
    int nxt = constrain(cur + delta, FR_MIN, FR_MAX);
    servoRear.writeDeg(nxt);
    Serial.printf("[MOVE] Rear : %d -> %d (delta %+d)\n", cur, nxt, delta);
  }
}

void printWheelHelp(){
  Serial.println(F("=== Wheel Commands (legacy) ==="));
  Serial.println(F("  ws=VAL      -> both speeds (-255..255)"));
  Serial.println(F("  wbrake      -> brake both"));
  Serial.println(F("  wstop       -> coast (free-roll)"));
  Serial.println(F("Web UI at your AP IP (see log)."));
}

void processWheelCommand(const String& cmd){
  if (cmd == "wstop") { wheels.coast(); Serial.println(F("[WHEEL] coast")); }
  else if (cmd == "wbrake") { wheels.brake(); Serial.println(F("[WHEEL] brake")); }
  else if (cmd.startsWith("ws=")) {
    int v = cmd.substring(3).toInt();
    wheels.setSpeedBoth(v);
    Serial.printf("[WHEEL] both=%d\n", v);
  } else {
    Serial.println(F("[WHEEL] Unknown. Use ws=, wstop, wbrake"));
  }
}

void processLineCommand(const String& cmd){
  if (cmd == "c") { centerSteer(); eStop=false; Serial.println(F("[CMD] Centered.")); }
  else if (cmd == "sf") { Serial.println(F("[CMD] Sweep FRONT 60..140")); servoFront.sweep(FF_MIN,FF_MAX,1,10); }
  else if (cmd == "sr") { Serial.println(F("[CMD] Sweep REAR  40..120")); servoRear.sweep(FR_MIN,FR_MAX,1,10); }
  else if (cmd.equalsIgnoreCase("A90")) { centerSteer(); eStop=false; }
  else if (cmd.length() && cmd[0]=='w') { processWheelCommand(cmd); }
  // Recorder serial with slot (optional number after command)
  else if (cmd.startsWith("recstart")) {
    int s = cmd.length()>8 ? cmd.substring(8).toInt() : 0;
    if (s<1 || s>REC_SLOTS) s = readNextSlot();
    char pJson[20], pMeta[20]; pathForSlot(s,pJson,sizeof(pJson),false); pathForSlot(s,pMeta,sizeof(pMeta),true);
    if (recorder.startRecording(pJson,pMeta)) {
      if (cmd.length()<=8) writeNextSlot(slotAfter(s)); // auto advances only if no explicit slot
      Serial.printf("[REC] start slot #%d\n", s);
    } else Serial.printf("[REC] ERR: %s\n", recorder.lastError());
  }
  else if (cmd=="recstop") { recorder.stopRecording(); Serial.println(F("[REC] stop")); }
  else if (cmd.startsWith("playr")) {
    int s = cmd.length()>5 ? cmd.substring(5).toInt() : 1;
    char pJson[20]; pathForSlot(s,pJson,sizeof(pJson),false);
    if (recorder.startPlayback(PLAY_REVERSE,pJson)) Serial.printf("[REC] play reverse slot #%d\n", s);
    else Serial.printf("[REC] ERR: %s\n", recorder.lastError());
  }
  else if (cmd.startsWith("play")) {
    int s = cmd.length()>4 ? cmd.substring(4).toInt() : 1;
    char pJson[20]; pathForSlot(s,pJson,sizeof(pJson),false);
    if (recorder.startPlayback(PLAY_FORWARD,pJson)) Serial.printf("[REC] play slot #%d\n", s);
    else Serial.printf("[REC] ERR: %s\n", recorder.lastError());
  }
  else if (cmd.startsWith("recclear")) {
    int s = cmd.length()>8 ? cmd.substring(8).toInt() : 1;
    char pJson[20], pMeta[20]; pathForSlot(s,pJson,sizeof(pJson),false); pathForSlot(s,pMeta,sizeof(pMeta),true);
    bool ok1=recorder.clearFile(pJson), ok2=recorder.clearFile(pMeta);
    Serial.println((ok1&&ok2)?F("[REC] cleared"):F("[REC] clear ERR"));
  }
  else if (cmd=="help") { printMenu(); printWheelHelp(); }
  else { Serial.println(F("[WARN] Unknown. Type 'help'.")); }
}

// -------- Setup & Loop --------
void setup(){
  Serial.begin(SERIAL_BAUD); delay(400);
  Serial.println(F("\n=== CamMate v3.5 – 5-slot Recorder Ring ==="));

  SPIFFS.begin(true); // recorder uses SPIFFS internally too

  // Servos
  servoFront.attach(SERVO_FRONT_PIN);
  servoRear.attach(SERVO_REAR_PIN);
  centerSteer();

  // Wheels
  WheelPins p{ L298_IN1, L298_IN2, L298_ENA, L298_IN3, L298_IN4, L298_ENB };
  wheels.begin(p, WHEEL_PWM_FREQ_HZ, WHEEL_PWM_BITS);
  Serial.println(F("[OK] Wheels ready."));

  // Recorder
  if (recorder.begin()) {
    recorder.setSampleMs(50); // 20 Hz
    Serial.printf("[REC] next auto slot: #%d\n", readNextSlot());
  } else {
    Serial.println(F("[REC] SPIFFS mount failed"));
  }

  // Network
  WiFi.mode(WIFI_AP);
  IPAddress apIP(AP_IP_OCT1,AP_IP_OCT2,AP_IP_OCT3,AP_IP_OCT4);
  IPAddress gw(AP_GW_OCT1,AP_GW_OCT2,AP_GW_OCT3,AP_GW_OCT4);
  IPAddress msk(AP_MASK_OCT1,AP_MASK_OCT2,AP_MASK_OCT3,AP_MASK_OCT4);
  WiFi.softAPConfig(apIP,gw,msk);
  WiFi.softAP(WIFI_SSID,WIFI_PASS);
  delay(200);

  // HTTP
  server.on("/",        HTTP_GET, handleIndex);
  server.on("/ctl",     HTTP_GET, handleCtl);
  server.on("/stop",    HTTP_GET, handleStop);
  server.on("/center",  HTTP_GET, handleCenter);
  server.on("/speed",   HTTP_GET, handleSpeed);

  server.on("/rec/list",  HTTP_GET, handleRecList);
  server.on("/rec/start", HTTP_GET, handleRecStart);
  server.on("/rec/stop",  HTTP_GET, handleRecStop);
  server.on("/rec/play",  HTTP_GET, handleRecPlay);
  server.on("/rec/clear", HTTP_GET, handleRecClear);

  server.begin();

  printMenu(); printWheelHelp();

  IPAddress ip = WiFi.softAPIP();
  Serial.printf("[UI] Connect '%s'/'%s' -> http://%s/\n", WIFI_SSID, WIFI_PASS, ip.toString().c_str());
}

void loop(){
  // Recorder playback applies frames into inputs
  recorder.tick(millis(), [](const RecFrame& fr){
    joyX       = fr.x;
    joyY       = fr.y;
    uiMode     = (UIMode)fr.mode;
    circleDiam = fr.diam;
    g_speedMode= (SpeedMode)fr.speed;
  });

  server.handleClient();

  // Serial quick keys
  while (Serial.available()) {
    int pk = Serial.peek();
    if (pk=='\n'||pk=='\r'){ Serial.read(); continue; }
    if (pk=='f'||pk=='r'||pk=='n'||pk=='m'||pk=='p') {
      char k=(char)Serial.read();
      if (k=='f') { Serial.read(); selectFront(); }
      else if (k=='r') { Serial.read(); selectRear(); }
      else if (k=='n') { Serial.read(); moveSelectedBy(-10); }
      else if (k=='m') { Serial.read(); moveSelectedBy(+10); }
      else if (k=='p') { Serial.read();
        g_speedMode = (g_speedMode==SPEED_LOW)?SPEED_NORMAL:(g_speedMode==SPEED_NORMAL?SPEED_SPORT:SPEED_LOW);
        Serial.printf("[SPEED] Mode: %s\n", g_speedMode==SPEED_SPORT?"SPORT":(g_speedMode==SPEED_LOW?"LOW":"NORMAL"));
      }
    } else break;
  }

  static String cmd;
  if (readLine(Serial, cmd)) { cmd.trim(); if (cmd.length()) processLineCommand(cmd); cmd=""; }

  // Planner & motors
  if (!eStop) {
    int ff, fr, base; float steerExtent;
    planSteering(joyX, joyY, uiMode, circleDiam, ff, fr, base, steerExtent);
    setFrontSteer(ff); setRearSteer(fr);
    int spd = applySpeedScaling(base, steerExtent);
    wheels.setSpeedBoth(constrain(spd, -255, 255));
  } else {
    wheels.setSpeedBoth(0);
  }
}
