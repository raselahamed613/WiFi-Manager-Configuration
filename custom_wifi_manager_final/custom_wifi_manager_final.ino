// ============================================================
//  Water Pump Controller — WiFi Manager + Device Config
//  Compatible with ESP32 & ESP8266
//
//  FLOW:
//    1. Boot → ESP opens AP (PumpCtrl_XXXXXX)
//    2. App connects to AP → calls GET /api/status to check
//    3. App calls GET /api/scan to get WiFi list
//    4. App calls POST /api/setup with ALL params at once:
//         { ssid, password, tankHeight, sensorGap,
//           pumpOnLevel, pumpOffLevel, autoMode,
//           timerValue, unitCostPrice }
//    5. ESP saves everything to flash → connects to WiFi
//    6. App can GET /api/config anytime to read saved values
//    7. App can POST /api/config to update device params only
// ============================================================

// ---------- Platform-specific headers ----------
#ifdef ESP32
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
WebServer server(80);
Preferences preferences;

#elif defined(ESP8266)
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
ESP8266WebServer server(80);

String readFile(const char *path) {
  File f = LittleFS.open(path, "r");
  if (!f)
    return "";
  String s = f.readString();
  f.close();
  return s;
}
void writeFile(const char *path, const String &data) {
  File f = LittleFS.open(path, "w");
  if (f) {
    f.print(data);
    f.close();
  }
}
#else
#error "Unsupported platform. Please use ESP32 or ESP8266."
#endif

#include <ArduinoJson.h>

// ============================================================
//  All configuration in ONE struct (WiFi + Device params)
// ============================================================
struct AllConfig {
  // WiFi
  String wifiSSID;
  String wifiPass;
  // Device
  float tankHeight;    // cm
  float sensorGap;     // cm
  float pumpOnLevel;   // %
  float pumpOffLevel;  // %
  bool autoMode;       // true=auto, false=manual
  float timerValue;    // min
  float unitCostPrice; // BDT/kWh
};

// Default values on first boot
AllConfig cfg = {"", "", 100.0, 5.0, 20.0, 90.0, true, 30.0, 8.0};

// ---------- Globals ----------
bool shouldConnect = false;
String apName = "";

// ============================================================
//  CORS helper — allows app to call API from any origin
// ============================================================
void addCORSHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void handleOptions() {
  addCORSHeaders();
  server.send(204);
}

// ============================================================
//  Flash Storage — Save & Load ALL config (ESP32 NVS / ESP8266 LittleFS)
// ============================================================
void flashSaveAll() {
#ifdef ESP32
  preferences.begin("pump-cfg", false);
  preferences.putString("ssid", cfg.wifiSSID);
  preferences.putString("pass", cfg.wifiPass);
  preferences.putFloat("tankH", cfg.tankHeight);
  preferences.putFloat("snsGap", cfg.sensorGap);
  preferences.putFloat("pumpOn", cfg.pumpOnLevel);
  preferences.putFloat("pumpOff", cfg.pumpOffLevel);
  preferences.putBool("autoM", cfg.autoMode);
  preferences.putFloat("timer", cfg.timerValue);
  preferences.putFloat("unitCost", cfg.unitCostPrice);
  preferences.end();
#elif defined(ESP8266)
  writeFile("/ssid.txt", cfg.wifiSSID);
  writeFile("/pass.txt", cfg.wifiPass);
  writeFile("/tankH.txt", String(cfg.tankHeight));
  writeFile("/snsGap.txt", String(cfg.sensorGap));
  writeFile("/pumpOn.txt", String(cfg.pumpOnLevel));
  writeFile("/pumpOff.txt", String(cfg.pumpOffLevel));
  writeFile("/autoM.txt", String(cfg.autoMode ? "1" : "0"));
  writeFile("/timer.txt", String(cfg.timerValue));
  writeFile("/unitCost.txt", String(cfg.unitCostPrice));
#endif
  Serial.println("[FLASH] All config saved to flash.");
}

void flashSaveDeviceOnly() {
#ifdef ESP32
  preferences.begin("pump-cfg", false);
  preferences.putFloat("tankH", cfg.tankHeight);
  preferences.putFloat("snsGap", cfg.sensorGap);
  preferences.putFloat("pumpOn", cfg.pumpOnLevel);
  preferences.putFloat("pumpOff", cfg.pumpOffLevel);
  preferences.putBool("autoM", cfg.autoMode);
  preferences.putFloat("timer", cfg.timerValue);
  preferences.putFloat("unitCost", cfg.unitCostPrice);
  preferences.end();
#elif defined(ESP8266)
  writeFile("/tankH.txt", String(cfg.tankHeight));
  writeFile("/snsGap.txt", String(cfg.sensorGap));
  writeFile("/pumpOn.txt", String(cfg.pumpOnLevel));
  writeFile("/pumpOff.txt", String(cfg.pumpOffLevel));
  writeFile("/autoM.txt", String(cfg.autoMode ? "1" : "0"));
  writeFile("/timer.txt", String(cfg.timerValue));
  writeFile("/unitCost.txt", String(cfg.unitCostPrice));
#endif
  Serial.println("[FLASH] Device config saved to flash.");
}

void flashLoadAll() {
#ifdef ESP32
  preferences.begin("pump-cfg", true);
  cfg.wifiSSID = preferences.getString("ssid", "");
  cfg.wifiPass = preferences.getString("pass", "");
  cfg.tankHeight = preferences.getFloat("tankH", cfg.tankHeight);
  cfg.sensorGap = preferences.getFloat("snsGap", cfg.sensorGap);
  cfg.pumpOnLevel = preferences.getFloat("pumpOn", cfg.pumpOnLevel);
  cfg.pumpOffLevel = preferences.getFloat("pumpOff", cfg.pumpOffLevel);
  cfg.autoMode = preferences.getBool("autoM", cfg.autoMode);
  cfg.timerValue = preferences.getFloat("timer", cfg.timerValue);
  cfg.unitCostPrice = preferences.getFloat("unitCost", cfg.unitCostPrice);
  preferences.end();
#elif defined(ESP8266)
  cfg.wifiSSID = readFile("/ssid.txt");
  cfg.wifiSSID.trim();
  cfg.wifiPass = readFile("/pass.txt");
  cfg.wifiPass.trim();
  String v;
  v = readFile("/tankH.txt");
  if (v.length())
    cfg.tankHeight = v.toFloat();
  v = readFile("/snsGap.txt");
  if (v.length())
    cfg.sensorGap = v.toFloat();
  v = readFile("/pumpOn.txt");
  if (v.length())
    cfg.pumpOnLevel = v.toFloat();
  v = readFile("/pumpOff.txt");
  if (v.length())
    cfg.pumpOffLevel = v.toFloat();
  v = readFile("/autoM.txt");
  if (v.length())
    cfg.autoMode = (v.toInt() == 1);
  v = readFile("/timer.txt");
  if (v.length())
    cfg.timerValue = v.toFloat();
  v = readFile("/unitCost.txt");
  if (v.length())
    cfg.unitCostPrice = v.toFloat();
#endif
}

// ============================================================
//  Wi-Fi connection helper
// ============================================================
bool connectWiFi(const String &ssid, const String &pass) {
  Serial.print("\n[WiFi] Connecting to: ");
  Serial.println(ssid);
  WiFi.begin(ssid.c_str(), pass.c_str());
  for (int i = 0; i < 30 && WiFi.status() != WL_CONNECTED; i++) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\n[WiFi] Connected! IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }
  Serial.println("\n[WiFi] Connection failed.");
  return false;
}

// ============================================================
//  HTML Portal — served at GET /
// ============================================================
const char PORTAL_HTML[] PROGMEM = R"HTMLPAGE(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>Water Pump Controller</title>
<style>
@import url('https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&display=swap');
:root{
  --bg:#0d1117;--surface:#161b22;--s2:#1f2937;
  --border:#30363d;--accent:#2563eb;--ok:#10b981;
  --warn:#f59e0b;--err:#ef4444;--text:#e6edf3;
  --muted:#8b949e;--r:12px;
}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Inter',sans-serif;background:var(--bg);color:var(--text);
  min-height:100vh;padding:16px}
.wrap{max-width:500px;margin:0 auto}

.hdr{text-align:center;padding:28px 0 20px}
.hdr .ico{font-size:44px;display:block;animation:bob 2.2s ease-in-out infinite}
@keyframes bob{0%,100%{transform:translateY(0)}50%{transform:translateY(-7px)}}
.hdr h1{font-size:22px;font-weight:700;margin-top:8px;
  background:linear-gradient(135deg,#2563eb,#7c3aed);
  -webkit-background-clip:text;-webkit-text-fill-color:transparent}
.hdr p{color:var(--muted);font-size:13px;margin-top:4px}

/* Status badge */
.badge{display:inline-flex;align-items:center;gap:6px;padding:5px 12px;
  border-radius:20px;font-size:12px;font-weight:600;margin-top:8px;border:1px solid}
.badge.ap{background:rgba(245,158,11,.1);border-color:var(--warn);color:var(--warn)}
.badge.sta{background:rgba(16,185,129,.1);border-color:var(--ok);color:var(--ok)}
.dot{width:7px;height:7px;border-radius:50%;background:currentColor;
  animation:pulse 1.4s ease-in-out infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.3}}

/* Tabs */
.tabs{display:flex;background:var(--surface);border-radius:var(--r);
  padding:4px;margin-bottom:18px;border:1px solid var(--border);gap:3px}
.tab{flex:1;padding:10px 6px;text-align:center;border-radius:8px;cursor:pointer;
  font-size:12px;font-weight:500;color:var(--muted);transition:all .25s;user-select:none}
.tab.active{background:var(--accent);color:#fff;box-shadow:0 2px 8px rgba(37,99,235,.4)}

/* Card */
.card{background:var(--surface);border:1px solid var(--border);border-radius:var(--r);
  padding:20px;margin-bottom:14px;transition:border-color .2s}
.card:hover{border-color:#3b82f6}
.ct{font-size:12px;font-weight:600;color:var(--muted);text-transform:uppercase;
  letter-spacing:.07em;margin-bottom:14px;display:flex;align-items:center;gap:7px}

/* Fields */
.f{margin-bottom:14px}
.f label{display:block;font-size:13px;font-weight:500;margin-bottom:5px}
.hint{font-size:11px;color:var(--muted);margin-top:4px}
input[type=number],input[type=text],input[type=password]{
  width:100%;padding:10px 13px;background:var(--s2);
  border:1px solid var(--border);border-radius:8px;color:var(--text);
  font-size:14px;font-family:inherit;outline:none;
  transition:border-color .2s,box-shadow .2s}
input:focus{border-color:var(--accent);box-shadow:0 0 0 3px rgba(37,99,235,.15)}

/* Toggle */
.tog{display:flex;align-items:center;justify-content:space-between;
  padding:11px 0;border-bottom:1px solid var(--border)}
.tog:last-child{border-bottom:none;padding-bottom:0}
.tog .inf p{font-size:14px;font-weight:500}
.tog .inf span{font-size:12px;color:var(--muted)}
.sw{position:relative;width:44px;height:24px;flex-shrink:0}
.sw input{opacity:0;width:0;height:0}
.sl{position:absolute;inset:0;background:#374151;border-radius:24px;
  cursor:pointer;transition:.3s}
.sl:before{content:"";position:absolute;height:18px;width:18px;
  left:3px;bottom:3px;background:#fff;border-radius:50%;transition:.3s}
input:checked+.sl{background:var(--ok)}
input:checked+.sl:before{transform:translateX(20px)}

/* Network list */
.nl{max-height:190px;overflow-y:auto;scrollbar-width:thin;
  scrollbar-color:var(--border) transparent}
.ni{display:flex;align-items:center;justify-content:space-between;
  padding:9px 11px;border-radius:8px;cursor:pointer;transition:background .2s;
  margin-bottom:2px;border:1px solid transparent}
.ni:hover,.ni.sel{background:rgba(37,99,235,.11);border-color:rgba(37,99,235,.3)}
.ns{font-size:14px;font-weight:500}
.nm{font-size:12px;color:var(--muted);display:flex;align-items:center;gap:5px}
.bars{display:inline-flex;gap:2px;align-items:flex-end}
.bars span{width:4px;border-radius:1px}

/* Buttons */
.btn{width:100%;padding:12px;border:none;border-radius:8px;font-family:inherit;
  font-size:14px;font-weight:600;cursor:pointer;transition:all .2s;
  display:flex;align-items:center;justify-content:center;gap:8px;margin-top:6px}
.btn-p{background:linear-gradient(135deg,#2563eb,#7c3aed);color:#fff}
.btn-p:hover{opacity:.9;transform:translateY(-1px);box-shadow:0 4px 16px rgba(37,99,235,.4)}
.btn-s{background:linear-gradient(135deg,#059669,#10b981);color:#fff}
.btn-s:hover{opacity:.9;transform:translateY(-1px);box-shadow:0 4px 16px rgba(16,185,129,.4)}
.btn-o{background:var(--s2);color:var(--text);border:1px solid var(--border);
  width:auto;padding:6px 13px;font-size:12px;margin-top:0;border-radius:6px}
.btn-o:hover{border-color:var(--accent);color:#60a5fa}

/* API reference table */
table{width:100%;border-collapse:collapse;font-size:12px}
th{background:var(--s2);padding:8px 10px;text-align:left;
  font-weight:600;color:var(--muted);border:1px solid var(--border)}
td{padding:8px 10px;border:1px solid var(--border)}
code{background:var(--s2);padding:2px 6px;border-radius:4px;
  font-size:11px;color:#60a5fa;font-family:monospace}

/* Toast */
.toast{position:fixed;top:16px;left:50%;
  transform:translateX(-50%) translateY(-90px);
  background:var(--surface);border:1px solid var(--border);
  border-radius:10px;padding:12px 20px;font-size:13px;font-weight:500;
  z-index:99;box-shadow:0 8px 28px rgba(0,0,0,.5);
  transition:transform .35s cubic-bezier(.34,1.56,.64,1)}
.toast.show{transform:translateX(-50%) translateY(0)}
.toast.ok{border-color:var(--ok);color:var(--ok)}
.toast.err{border-color:var(--err);color:var(--err)}

.panel{display:none}.panel.active{display:block}
.spin{display:inline-block;width:13px;height:13px;
  border:2px solid rgba(255,255,255,.3);border-top-color:#fff;
  border-radius:50%;animation:sp .7s linear infinite}
@keyframes sp{to{transform:rotate(360deg)}}
.hide{display:none!important}
.sr{display:flex;align-items:center;justify-content:space-between;margin-bottom:10px}
</style>
</head>
<body>
<div class="wrap">

<div class="hdr">
  <span class="ico">💧</span>
  <h1>Water Pump Controller</h1>
  <p>WiFi Setup &amp; Device Configuration</p>
  <div id="statusBadge" class="badge ap">
    <span class="dot"></span><span id="statusTxt">AP Mode — Not Connected</span>
  </div>
</div>

<div class="tabs">
  <div class="tab active" id="t1" onclick="showTab(1)">📶 WiFi</div>
  <div class="tab" id="t2" onclick="showTab(2)">⚙️ Device</div>
  <div class="tab" id="t3" onclick="showTab(3)">📋 API Docs</div>
</div>

<!-- ===== WiFi Tab ===== -->
<div class="panel active" id="p1">
  <div class="card">
    <div class="sr">
      <label class="ct" style="margin:0">📡 Available Networks</label>
      <button class="btn btn-o" id="scanBtn" onclick="doScan()">🔍 Scan</button>
    </div>
    <div class="nl" id="netList">
      <p style="color:var(--muted);font-size:13px;text-align:center;padding:18px 0">
        Tap Scan to find WiFi networks
      </p>
    </div>
  </div>
  <div class="card">
    <div class="ct">🔐 WiFi Credentials</div>
    <div class="f">
      <label>SSID</label>
      <input type="text" id="ssid" placeholder="Select above or type manually"/>
    </div>
    <div class="f">
      <label>Password</label>
      <input type="password" id="wpass" placeholder="WiFi password"/>
    </div>
    <p style="font-size:12px;color:var(--muted);margin-top:4px">
      💡 Fill Device Config tab first, then press Setup All below to save everything at once.
    </p>
    <button class="btn btn-p" onclick="doSetupAll()" id="setupBtn">
      <span id="setupSpin" class="spin hide"></span>
      🚀 Setup All &amp; Connect
    </button>
  </div>
</div>

<!-- ===== Device Config Tab ===== -->
<div class="panel" id="p2">
  <div class="card">
    <div class="ct">🏗️ Tank &amp; Sensor</div>
    <div class="f">
      <label>Tank Height (cm)</label>
      <input type="number" id="tankHeight" min="1" max="1000" step="0.5" placeholder="e.g. 100"/>
      <div class="hint">Total height of your water tank</div>
    </div>
    <div class="f">
      <label>Sensor Gap (cm)</label>
      <input type="number" id="sensorGap" min="0" max="100" step="0.5" placeholder="e.g. 5"/>
      <div class="hint">Distance from sensor face to tank top</div>
    </div>
  </div>
  <div class="card">
    <div class="ct">🔧 Pump Level Control</div>
    <div class="f">
      <label>Pump ON Level (%)</label>
      <input type="number" id="pumpOn" min="0" max="100" step="1" placeholder="e.g. 20"/>
      <div class="hint">Pump starts when water drops below this level</div>
    </div>
    <div class="f">
      <label>Pump OFF Level (%)</label>
      <input type="number" id="pumpOff" min="0" max="100" step="1" placeholder="e.g. 90"/>
      <div class="hint">Pump stops when water reaches this level</div>
    </div>
  </div>
  <div class="card">
    <div class="ct">🕹️ Operation Mode</div>
    <div class="tog">
      <div class="inf"><p>Automatic Mode</p>
        <span>Control pump automatically by water level</span></div>
      <label class="sw"><input type="checkbox" id="autoMode" checked/>
        <span class="sl"></span></label>
    </div>
    <div class="f" style="margin-top:14px">
      <label>Timer Value (min)</label>
      <input type="number" id="timerVal" min="1" max="1440" step="1" placeholder="e.g. 30"/>
      <div class="hint">Run duration in manual/timer mode</div>
    </div>
  </div>
  <div class="card">
    <div class="ct">💰 Cost</div>
    <div class="f">
      <label>Unit Cost Price (BDT / kWh)</label>
      <input type="number" id="unitCost" min="0" max="999" step="0.01" placeholder="e.g. 8.00"/>
      <div class="hint">Electricity cost for consumption estimation</div>
    </div>
  </div>
  <button class="btn btn-s" onclick="doCfgOnly()" id="cfgBtn">
    <span id="cfgSpin" class="spin hide"></span>
    💾 Save Device Config Only
  </button>
</div>

<!-- ===== API Docs Tab ===== -->
<div class="panel" id="p3">
  <div class="card">
    <div class="ct">📋 REST API Reference</div>
    <p style="font-size:12px;color:var(--muted);margin-bottom:14px">
      Use these endpoints from your mobile app when connected to the ESP AP.
      Base URL: <code>http://192.168.4.1</code>
    </p>
    <table>
      <tr><th>Method</th><th>Endpoint</th><th>Description</th></tr>
      <tr><td><code>GET</code></td><td><code>/api/status</code></td><td>Device status &amp; IP</td></tr>
      <tr><td><code>GET</code></td><td><code>/api/scan</code></td><td>Scan WiFi networks</td></tr>
      <tr><td><code>POST</code></td><td><code>/api/setup</code></td><td>Save ALL config + connect WiFi</td></tr>
      <tr><td><code>GET</code></td><td><code>/api/config</code></td><td>Read device config</td></tr>
      <tr><td><code>POST</code></td><td><code>/api/config</code></td><td>Update device config only</td></tr>
    </table>
  </div>
  <div class="card">
    <div class="ct">🚀 POST /api/setup — Full Setup Body</div>
    <pre style="background:var(--s2);padding:14px;border-radius:8px;font-size:12px;
      color:#a5f3fc;overflow-x:auto;line-height:1.7">{
  "ssid":         "YourWiFi",
  "password":     "YourPass",
  "tankHeight":   100.0,
  "sensorGap":    5.0,
  "pumpOnLevel":  20.0,
  "pumpOffLevel": 90.0,
  "autoMode":     true,
  "timerValue":   30.0,
  "unitCostPrice":8.0
}</pre>
  </div>
  <div class="card">
    <div class="ct">⚙️ POST /api/config — Device Config Only</div>
    <pre style="background:var(--s2);padding:14px;border-radius:8px;font-size:12px;
      color:#a5f3fc;overflow-x:auto;line-height:1.7">{
  "tankHeight":   100.0,
  "sensorGap":    5.0,
  "pumpOnLevel":  20.0,
  "pumpOffLevel": 90.0,
  "autoMode":     true,
  "timerValue":   30.0,
  "unitCostPrice":8.0
}</pre>
  </div>
</div>

</div><!-- /wrap -->
<div class="toast" id="toast"></div>

<script>
// ---- Tab ----
function showTab(n){
  for(var i=1;i<=3;i++){
    document.getElementById('t'+i).classList.toggle('active',i===n);
    document.getElementById('p'+i).classList.toggle('active',i===n);
  }
  if(n===2) loadCfg();
  if(n===1) checkStatus();
}

// ---- Toast ----
function toast(msg,type){
  var el=document.getElementById('toast');
  el.textContent=msg; el.className='toast '+(type||'ok');
  el.classList.add('show');
  setTimeout(function(){el.classList.remove('show');},3800);
}

// ---- Status ----
function checkStatus(){
  fetch('/api/status').then(function(r){return r.json();}).then(function(d){
    var b=document.getElementById('statusBadge');
    var t=document.getElementById('statusTxt');
    if(d.mode==='STA'&&d.connected){
      b.className='badge sta';
      t.textContent='Connected — '+d.ip;
    } else {
      b.className='badge ap';
      t.textContent='AP Mode — '+d.apSSID;
    }
  }).catch(function(){});
}

// ---- RSSI bars ----
function bars(rssi){
  var c=rssi>=-60?'#10b981':rssi>=-75?'#f59e0b':'#ef4444';
  var n=rssi>=-60?4:rssi>=-75?3:2;
  var h=[5,8,11,14]; var s='<span class="bars">';
  for(var i=0;i<4;i++){
    s+='<span style="height:'+h[i]+'px;background:'+(i<n?c:'#374151')+'"></span>';
  }
  return s+'</span>';
}

// ---- Scan ----
function doScan(){
  var btn=document.getElementById('scanBtn');
  btn.disabled=true; btn.textContent='⏳';
  document.getElementById('netList').innerHTML=
    '<p style="color:var(--muted);font-size:13px;text-align:center;padding:18px 0">Scanning...</p>';
  fetch('/api/scan').then(function(r){return r.json();}).then(function(nets){
    btn.disabled=false; btn.textContent='🔍 Scan';
    if(!nets.length){
      document.getElementById('netList').innerHTML=
        '<p style="color:var(--muted);font-size:13px;text-align:center;padding:18px 0">No networks found.</p>';
      return;
    }
    nets.sort(function(a,b){return b.rssi-a.rssi;});
    document.getElementById('netList').innerHTML=nets.map(function(n){
      return '<div class="ni" onclick="selNet(\''+n.ssid.replace(/\'/g,"\\'")+'\')"><span class="ns">'+n.ssid+'</span>'
        +'<span class="nm">'+bars(n.rssi)+' '+n.rssi+'dBm '+(n.encryption==='Open'?'🔓':'🔒')+'</span></div>';
    }).join('');
  }).catch(function(){
    btn.disabled=false; btn.textContent='🔍 Scan'; toast('Scan failed','err');
  });
}

function selNet(s){
  document.getElementById('ssid').value=s;
  document.querySelectorAll('.ni').forEach(function(el){
    el.classList.toggle('sel',el.querySelector('.ns').textContent===s);
  });
}

// ---- Collect device config fields ----
function getCfgFields(){
  return {
    tankHeight:   parseFloat(document.getElementById('tankHeight').value)||0,
    sensorGap:    parseFloat(document.getElementById('sensorGap').value)||0,
    pumpOnLevel:  parseFloat(document.getElementById('pumpOn').value)||0,
    pumpOffLevel: parseFloat(document.getElementById('pumpOff').value)||0,
    autoMode:     document.getElementById('autoMode').checked,
    timerValue:   parseFloat(document.getElementById('timerVal').value)||0,
    unitCostPrice:parseFloat(document.getElementById('unitCost').value)||0
  };
}

// ---- POST /api/setup — ALL config at once ----
function doSetupAll(){
  var ssid=document.getElementById('ssid').value.trim();
  if(!ssid){toast('Please enter or select an SSID','err');return;}
  var c=getCfgFields();
  if(c.tankHeight<=0){toast('Go to Device tab and set Tank Height','err');return;}
  if(c.pumpOnLevel>=c.pumpOffLevel){toast('Pump ON % must be less than Pump OFF %','err');return;}
  var btn=document.getElementById('setupBtn');
  var sp=document.getElementById('setupSpin');
  btn.disabled=true; sp.classList.remove('hide');
  var body=Object.assign({ssid:ssid,password:document.getElementById('wpass').value},c);
  fetch('/api/setup',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify(body)})
  .then(function(r){return r.json();})
  .then(function(d){
    toast(d.message||'Setup saved! Connecting...','ok');
    setTimeout(checkStatus,4000);
  })
  .catch(function(){toast('Saved! ESP is connecting to WiFi...','ok');setTimeout(checkStatus,5000);})
  .finally(function(){btn.disabled=false;sp.classList.add('hide');});
}

// ---- Load config from ESP ----
function loadCfg(){
  fetch('/api/config').then(function(r){return r.json();}).then(function(d){
    if(d.tankHeight!=null)   document.getElementById('tankHeight').value=d.tankHeight;
    if(d.sensorGap!=null)    document.getElementById('sensorGap').value=d.sensorGap;
    if(d.pumpOnLevel!=null)  document.getElementById('pumpOn').value=d.pumpOnLevel;
    if(d.pumpOffLevel!=null) document.getElementById('pumpOff').value=d.pumpOffLevel;
    if(d.autoMode!=null)     document.getElementById('autoMode').checked=d.autoMode;
    if(d.timerValue!=null)   document.getElementById('timerVal').value=d.timerValue;
    if(d.unitCostPrice!=null)document.getElementById('unitCost').value=d.unitCostPrice;
  }).catch(function(){});
}

// ---- POST /api/config — device params only ----
function doCfgOnly(){
  var c=getCfgFields();
  if(c.tankHeight<=0){toast('Tank height must be > 0','err');return;}
  if(c.pumpOnLevel>=c.pumpOffLevel){toast('Pump ON % must be less than Pump OFF %','err');return;}
  var btn=document.getElementById('cfgBtn');
  var sp=document.getElementById('cfgSpin');
  btn.disabled=true; sp.classList.remove('hide');
  fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(c)})
  .then(function(r){return r.json();})
  .then(function(d){toast(d.message||'Device config saved!','ok');})
  .catch(function(){toast('Failed — check connection','err');})
  .finally(function(){btn.disabled=false;sp.classList.add('hide');});
}

// Auto-check status on load
checkStatus();
</script>
</body>
</html>
)HTMLPAGE";

// ============================================================
//  API: GET /api/status — device status for app
// ============================================================
void handleApiStatus() {
  addCORSHeaders();
  JsonDocument doc;
  bool connected = (WiFi.status() == WL_CONNECTED);
  doc["connected"] = connected;
  doc["mode"] = connected ? "STA" : "AP";
  doc["ip"] =
      connected ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  doc["apSSID"] = apName;
  doc["rssi"] = connected ? WiFi.RSSI() : 0;
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// ============================================================
//  API: GET /api/scan — nearby networks
// ============================================================
void handleApiScan() {
  addCORSHeaders();
  int n = WiFi.scanNetworks();
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < n; ++i) {
    JsonObject net = arr.add<JsonObject>();
    net["ssid"] = WiFi.SSID(i);
    net["rssi"] = WiFi.RSSI(i);
#ifdef ESP32
    bool isOpen = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
#elif defined(ESP8266)
    bool isOpen = (WiFi.encryptionType(i) == ENC_TYPE_NONE);
#endif
    net["encryption"] = isOpen ? "Open" : "Secured";
  }
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// ============================================================
//  API: POST /api/setup — receive ALL config at once
//  Body: { ssid, password, tankHeight, sensorGap,
//          pumpOnLevel, pumpOffLevel, autoMode,
//          timerValue, unitCostPrice }
//  → Saves everything to flash, then connects WiFi
// ============================================================
void handleApiSetup() {
  addCORSHeaders();
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"Missing body\"}");
    return;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  // WiFi credentials
  if (doc["ssid"].is<const char *>())
    cfg.wifiSSID = doc["ssid"].as<String>();
  if (doc["password"].is<const char *>())
    cfg.wifiPass = doc["password"].as<String>();

  // Device params (use current value as fallback)
  cfg.tankHeight = doc["tankHeight"] | cfg.tankHeight;
  cfg.sensorGap = doc["sensorGap"] | cfg.sensorGap;
  cfg.pumpOnLevel = doc["pumpOnLevel"] | cfg.pumpOnLevel;
  cfg.pumpOffLevel = doc["pumpOffLevel"] | cfg.pumpOffLevel;
  cfg.autoMode = doc["autoMode"] | cfg.autoMode;
  cfg.timerValue = doc["timerValue"] | cfg.timerValue;
  cfg.unitCostPrice = doc["unitCostPrice"] | cfg.unitCostPrice;

  // Save everything to flash
  flashSaveAll();

  server.send(200, "application/json",
              "{\"status\":\"success\","
              "\"message\":\"All config saved! ESP connecting to WiFi...\"}");

  shouldConnect = true; // Deferred — runs after HTTP response is sent

  // Print full config table to Serial Monitor
  // printConfigDebug("SETUP RECEIVED — All Config Saved");
  Serial.println("SETUP RECEIVED — All Config Saved");
}

// ============================================================
//  API: GET /api/config — return current device config
// ============================================================
void handleApiConfigGet() {
  addCORSHeaders();
  JsonDocument doc;
  doc["tankHeight"] = cfg.tankHeight;
  doc["sensorGap"] = cfg.sensorGap;
  doc["pumpOnLevel"] = cfg.pumpOnLevel;
  doc["pumpOffLevel"] = cfg.pumpOffLevel;
  doc["autoMode"] = cfg.autoMode;
  doc["timerValue"] = cfg.timerValue;
  doc["unitCostPrice"] = cfg.unitCostPrice;
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// ============================================================
//  API: POST /api/config — update device params only (no WiFi change)
//  Body: { tankHeight, sensorGap, pumpOnLevel, pumpOffLevel,
//          autoMode, timerValue, unitCostPrice }
// ============================================================
void handleApiConfigPost() {
  addCORSHeaders();
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"Missing body\"}");
    return;
  }
  JsonDocument doc;
  deserializeJson(doc, server.arg("plain"));

  cfg.tankHeight = doc["tankHeight"] | cfg.tankHeight;
  cfg.sensorGap = doc["sensorGap"] | cfg.sensorGap;
  cfg.pumpOnLevel = doc["pumpOnLevel"] | cfg.pumpOnLevel;
  cfg.pumpOffLevel = doc["pumpOffLevel"] | cfg.pumpOffLevel;
  cfg.autoMode = doc["autoMode"] | cfg.autoMode;
  cfg.timerValue = doc["timerValue"] | cfg.timerValue;
  cfg.unitCostPrice = doc["unitCostPrice"] | cfg.unitCostPrice;

  flashSaveDeviceOnly();

  server.send(
      200, "application/json",
      "{\"status\":\"success\",\"message\":\"Device config updated!\"}");

  // Print full config table to Serial Monitor
  // printConfigDebug("CONFIG UPDATED — Device Params Saved");
  Serial.println("CONFIG UPDATED — Device Params Saved");
}

// ============================================================
//  Serve portal HTML
// ============================================================
void handleRoot() { server.send_P(200, "text/html", PORTAL_HTML); }

// ============================================================
//  Register all HTTP routes
// ============================================================
void registerRoutes() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/", HTTP_OPTIONS, handleOptions);
  server.on("/api/status", HTTP_GET, handleApiStatus);
  server.on("/api/scan", HTTP_GET, handleApiScan);
  server.on("/api/setup", HTTP_POST, handleApiSetup);
  server.on("/api/setup", HTTP_OPTIONS, handleOptions);
  server.on("/api/config", HTTP_GET, handleApiConfigGet);
  server.on("/api/config", HTTP_POST, handleApiConfigPost);
  server.on("/api/config", HTTP_OPTIONS, handleOptions);
}

// ============================================================
//  Start AP Config Portal
// ============================================================
void startConfigPortal() {
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  apName = "PumpCtrl_" + mac.substring(6, 12);

  WiFi.mode(WIFI_AP_STA); // AP+STA allows scanning while in AP mode
  WiFi.softAP(apName.c_str());

  Serial.println("\n=== Water Pump Controller — Config Portal ===");
  Serial.print("  AP SSID    : ");
  Serial.println(apName);
  Serial.print("  Portal URL : http://");
  Serial.println(WiFi.softAPIP());
  Serial.print("  API Base   : http://");
  Serial.print(WiFi.softAPIP());
  Serial.println("/api");
  Serial.println("=============================================");

  registerRoutes();
  server.begin();
}

// ============================================================
//  Debug: Print ALL config to Serial Monitor in a nice table
// ============================================================
void printConfigDebug(const char *title) {
  Serial.println();
  Serial.println("╔══════════════════════════════════════════╗");
  Serial.print  ("║  "); Serial.print(title);
  // Pad to width
  int pad = 40 - strlen(title);
  for (int i = 0; i < pad; i++) Serial.print(' ');
  Serial.println(" ║");
  Serial.println("╠══════════════════════════════════════════╣");
  Serial.println("║  [WiFi]                                  ║");
  Serial.printf ("║    SSID         : %-22s ║\n", cfg.wifiSSID.length() ? cfg.wifiSSID.c_str() : "(not set)");
  Serial.printf ("║    Password     : %-22s ║\n", cfg.wifiPass.length() ? "**hidden**" : "(not set)");
  Serial.println("╠══════════════════════════════════════════╣");
  Serial.println("║  [Tank & Sensor]                         ║");
  Serial.printf ("║    Tank Height  : %-6.1f cm               ║\n", cfg.tankHeight);
  Serial.printf ("║    Sensor Gap   : %-6.1f cm               ║\n", cfg.sensorGap);
  Serial.println("╠══════════════════════════════════════════╣");
  Serial.println("║  [Pump Control]                          ║");
  Serial.printf ("║    Pump ON  At  : %-5.1f %%                ║\n", cfg.pumpOnLevel);
  Serial.printf ("║    Pump OFF At  : %-5.1f %%                ║\n", cfg.pumpOffLevel);
  Serial.println("╠══════════════════════════════════════════╣");
  Serial.println("║  [Operation]                             ║");
  Serial.printf ("║    Auto Mode    : %-22s ║\n", cfg.autoMode ? "ON  (Automatic)" : "OFF (Manual)");
  Serial.printf ("║    Timer Value  : %-5.0f min               ║\n", cfg.timerValue);
  Serial.println("╠══════════════════════════════════════════╣");
  Serial.println("║  [Cost]                                  ║");
  Serial.printf ("║    Unit Cost    : %-6.2f BDT/kWh          ║\n", cfg.unitCostPrice);
  Serial.println("╚══════════════════════════════════════════╝");
  Serial.println();
}

// ============================================================
//  Setup
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(400);

  Serial.println();
  Serial.println("===========================================");
  Serial.println("   Water Pump Controller — Booting...   ");
  Serial.println("===========================================");

#ifdef ESP32
  Serial.println("[PLATFORM] ESP32 detected.");
#elif defined(ESP8266)
  Serial.println("[PLATFORM] ESP8266 detected.");
  if (!LittleFS.begin()) {
    Serial.println("[LittleFS] Formatting filesystem...");
    LittleFS.format();
    LittleFS.begin();
  }
  Serial.println("[LittleFS] Filesystem mounted OK.");
#endif

  // ── Load all config from flash ──
  Serial.println("[FLASH] Reading config from flash memory...");
  flashLoadAll();

  // ── Show all loaded config in Serial Monitor ──
  printConfigDebug("BOOT — Config Loaded from Flash");

  // ── Try saved WiFi ──
  if (cfg.wifiSSID.length() > 0) {
    Serial.println("[WiFi] Saved credentials found. Attempting connection...");
    WiFi.mode(WIFI_STA);
    if (connectWiFi(cfg.wifiSSID, cfg.wifiPass)) {
      Serial.println("[SYSTEM] Running in MAIN MODE (WiFi connected).");
      registerRoutes();
      server.begin();
      Serial.print("[HTTP] Config API available at http://");
      Serial.println(WiFi.localIP());
      Serial.println("[READY] Device is ready. Open Serial Monitor for live updates.");
      return;
    }
    Serial.println("[WiFi] Saved credentials FAILED. Opening AP config portal...");
  } else {
    Serial.println("[WiFi] No saved credentials found. Starting AP config portal...");
  }

  // ── No credentials or connection failed → start AP portal ──
  startConfigPortal();
  Serial.println("[READY] AP Portal active. Waiting for app connection...");
}

// ============================================================
//  Loop
// ============================================================
void loop() {
  server.handleClient();

#ifdef ESP8266
  yield();
#endif

  // Deferred WiFi connect after POST /api/setup response is sent
  if (shouldConnect) {
    shouldConnect = false;
    delay(1000); // Allow HTTP response to reach the app

    server.stop();
    WiFi.softAPdisconnect(true);

    if (connectWiFi(cfg.wifiSSID, cfg.wifiPass)) {
      WiFi.mode(WIFI_STA);
      registerRoutes();
      server.begin();
      Serial.print("[HTTP] Now accessible at http://");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("[PORTAL] Connection failed — reopening AP portal...");
      startConfigPortal();
    }
  }
}