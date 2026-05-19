// ============================================================
// ESP32 功能丰富的 Web 控制面板
// 适用开发板：ESP32 Dev Module
// 功能：LED控制 / 亮度调节 / 温湿度监控 / 系统状态 / 模式切换 / OTA升级
// ============================================================

// ==================== 配置区 ====================

// Wi-Fi 配置（修改这里连接你的路由器）
const char* ssid = "WIFI";
const char* password = "999999999";

// DHT11 温湿度传感器（需要 DHT sensor library）
// 启用传感器（已开启）
#define USE_DHT
#define DHT_PIN 4
#define DHT_TYPE DHT11

// LED 引脚
const int LED_PIN = 2;

// AP 模式开关（true 则不连接 Wi-Fi，直接发射热点）
const bool USE_AP_MODE = false;
const char* ap_ssid = "ESP32_Control";
const char* ap_password = "12345678";

// MQTT 配置（云端 Broker，手机通过 4G 订阅即可远程查看）
const char* mqtt_broker = "broker.emqx.io";
const int mqtt_port = 1883;
const char* mqtt_topic_temp = "esp32/temp";
const char* mqtt_topic_hum = "esp32/hum";
const char* mqtt_topic_status = "esp32/status";
const char* mqtt_topic_led = "esp32/led";
const char* mqtt_topic_cmd = "esp32/cmd";
const unsigned long MQTT_INTERVAL = 5000;  // 每 5 秒发布一次

// HTTP POST 上报（存入 PHP 虚拟主机 MySQL）
const char* HTTP_REPORT_URL = "https://www.sseeee.com/esp32/mmq/receiver.php";

// PushDeer 推送配置（多个 Key 逗号分隔，Android + iOS 同时收到）
const char* PUSHDEER_KEY = "PDU41451T5iKoPmpeiumcfCkvMOYBMnFsN2NGEG7z,PDU41456TsHlumkjoNeKlr07pPIT3A2xnioEtqiDY";
const float WX_ALERT_TEMP = 30.0;     // 超过此温度触发微信推送
const unsigned long WX_COOLDOWN = 600000;  // 10 分钟内不重复推送
unsigned long lastWxAlert = 0;

// ==================== 引入库 ====================

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <ArduinoOTA.h>
#ifdef USE_DHT
#include <DHT.h>
#endif

// ==================== 全局变量 ====================

#ifdef USE_DHT
DHT dht(DHT_PIN, DHT_TYPE);
#endif

AsyncWebServer server(80);

// 运行时状态
unsigned long startTime = 0;
int ledBrightness = 128;  // 0-255
bool ledState = true;
String currentMode = "auto";

// PWM 配置
const int PWM_FREQ = 1000;
const int PWM_RESOLUTION = 8;

// MQTT 客户端
WiFiClient espClient;
PubSubClient mqttClient(espClient);
unsigned long lastMqttPublish = 0;

// ==================== 网页界面（HTML/CSS/JS） ====================

const char INDEX_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 控制面板</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }

    :root {
      --bg: #0f0f1a;
      --card: #1a1a2e;
      --card-hover: #22223a;
      --accent: #6c5ce7;
      --accent-glow: rgba(108, 92, 231, 0.3);
      --green: #00cec9;
      --red: #ff6b6b;
      --orange: #fd9644;
      --text: #dcdde1;
      --text-dim: #636e72;
      --border: #2d2d4a;
      --shadow: rgba(0,0,0,0.4);
    }

    body {
      font-family: -apple-system, 'Segoe UI', system-ui, sans-serif;
      background: var(--bg);
      color: var(--text);
      min-height: 100vh;
      padding: 20px;
    }

    .container {
      max-width: 960px;
      margin: 0 auto;
    }

    /* ---- 顶部标题栏 ---- */
    .header {
      text-align: center;
      margin-bottom: 30px;
      padding: 30px 20px;
      background: var(--card);
      border-radius: 20px;
      border: 1px solid var(--border);
      box-shadow: 0 8px 32px var(--shadow);
      position: relative;
      overflow: hidden;
    }

    .header::before {
      content: '';
      position: absolute;
      top: 0; left: 0; right: 0;
      height: 3px;
      background: linear-gradient(90deg, var(--accent), var(--green), var(--orange));
    }

    .header h1 {
      font-size: 1.8rem;
      font-weight: 700;
      background: linear-gradient(135deg, var(--accent), var(--green));
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
      margin-bottom: 8px;
    }

    .header .status-bar {
      display: flex;
      justify-content: center;
      gap: 24px;
      flex-wrap: wrap;
      margin-top: 12px;
    }

    .status-item {
      display: flex;
      align-items: center;
      gap: 6px;
      font-size: 0.85rem;
      color: var(--text-dim);
    }

    .status-dot {
      width: 8px;
      height: 8px;
      border-radius: 50%;
      background: var(--green);
      animation: pulse 2s infinite;
    }

    @keyframes pulse {
      0%, 100% { opacity: 1; }
      50% { opacity: 0.4; }
    }

    /* ---- 卡片网格 ---- */
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
      gap: 20px;
      margin-bottom: 20px;
    }

    .card {
      background: var(--card);
      border-radius: 16px;
      border: 1px solid var(--border);
      padding: 24px;
      transition: transform 0.2s, box-shadow 0.2s;
      box-shadow: 0 4px 16px var(--shadow);
    }

    .card:hover {
      transform: translateY(-2px);
      box-shadow: 0 8px 24px var(--shadow);
    }

    .card-title {
      font-size: 0.8rem;
      text-transform: uppercase;
      letter-spacing: 1.5px;
      color: var(--text-dim);
      margin-bottom: 16px;
      display: flex;
      align-items: center;
      gap: 8px;
    }

    .card-title .icon {
      font-size: 1rem;
    }

    /* ---- LED 控制 ---- */
    .led-display {
      width: 80px;
      height: 80px;
      border-radius: 50%;
      margin: 0 auto 20px;
      border: 3px solid var(--border);
      transition: all 0.3s;
      display: flex;
      align-items: center;
      justify-content: center;
      font-size: 1.8rem;
    }

    .led-display.on {
      background: radial-gradient(circle, #fff176 0%, #ffd54f 40%, #ffb300 100%);
      border-color: #ffd54f;
      box-shadow: 0 0 30px rgba(255, 213, 79, 0.6), 0 0 60px rgba(255, 213, 79, 0.3);
    }

    .led-display.off {
      background: #2d2d4a;
      border-color: #3d3d5a;
    }

    .led-display span { opacity: 0.3; }

    /* 开关按钮 */
    .toggle-btn {
      display: block;
      width: 100%;
      padding: 14px;
      border: none;
      border-radius: 12px;
      font-size: 1rem;
      font-weight: 600;
      cursor: pointer;
      transition: all 0.3s;
      margin-bottom: 16px;
    }

    .toggle-btn.on {
      background: linear-gradient(135deg, var(--accent), #8b5cf6);
      color: white;
      box-shadow: 0 4px 15px var(--accent-glow);
    }

    .toggle-btn.off {
      background: #2d2d4a;
      color: var(--text-dim);
      border: 1px solid var(--border);
    }

    .toggle-btn:hover { transform: scale(1.02); }

    /* 亮度滑块 */
    .slider-wrap { margin-top: 8px; }
    .slider-label {
      display: flex;
      justify-content: space-between;
      font-size: 0.8rem;
      color: var(--text-dim);
      margin-bottom: 8px;
    }

    input[type="range"] {
      -webkit-appearance: none;
      width: 100%;
      height: 6px;
      border-radius: 3px;
      background: linear-gradient(90deg, var(--accent), var(--green));
      outline: none;
      cursor: pointer;
    }

    input[type="range"]::-webkit-slider-thumb {
      -webkit-appearance: none;
      width: 20px;
      height: 20px;
      border-radius: 50%;
      background: white;
      box-shadow: 0 2px 6px rgba(0,0,0,0.3);
      cursor: pointer;
    }

    /* ---- 温湿度卡片 ---- */
    .sensor-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 16px;
    }

    .sensor-box {
      background: rgba(255,255,255,0.03);
      border-radius: 12px;
      padding: 16px;
      text-align: center;
      border: 1px solid var(--border);
    }

    .sensor-icon { font-size: 1.6rem; margin-bottom: 8px; }

    .sensor-value {
      font-size: 2rem;
      font-weight: 700;
      color: var(--accent);
    }

    .sensor-value.temp { color: var(--orange); }
    .sensor-value.humidity { color: var(--green); }

    .sensor-label { font-size: 0.75rem; color: var(--text-dim); margin-top: 4px; }

    /* ---- 系统状态 ---- */
    .stat-grid {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 12px;
    }

    .stat-box {
      background: rgba(255,255,255,0.03);
      border-radius: 12px;
      padding: 14px;
      text-align: center;
      border: 1px solid var(--border);
    }

    .stat-value {
      font-size: 1.2rem;
      font-weight: 700;
      color: var(--accent);
      margin-bottom: 4px;
    }

    .stat-label { font-size: 0.7rem; color: var(--text-dim); text-transform: uppercase; letter-spacing: 1px; }

    /* Wi-Fi 强度条 */
    .wifi-bars {
      display: flex;
      align-items: flex-end;
      gap: 3px;
      justify-content: center;
      height: 30px;
      margin-bottom: 4px;
    }

    .wifi-bar {
      width: 6px;
      border-radius: 2px;
      background: var(--border);
      transition: background 0.3s;
    }

    .wifi-bar.active { background: var(--green); }
    .wifi-bar:nth-child(1) { height: 8px; }
    .wifi-bar:nth-child(2) { height: 14px; }
    .wifi-bar:nth-child(3) { height: 20px; }
    .wifi-bar:nth-child(4) { height: 26px; }

    /* ---- 快捷指令 ---- */
    .quick-grid {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 10px;
    }

    .quick-btn {
      padding: 12px;
      border: 1px solid var(--border);
      border-radius: 10px;
      background: rgba(255,255,255,0.03);
      color: var(--text);
      font-size: 0.85rem;
      cursor: pointer;
      transition: all 0.2s;
      text-align: center;
    }

    .quick-btn:hover {
      background: var(--accent-glow);
      border-color: var(--accent);
      transform: scale(1.03);
    }

    .quick-btn .qi { font-size: 1.2rem; display: block; margin-bottom: 4px; }

    /* ---- 模式选择 ---- */
    .mode-tabs {
      display: flex;
      background: rgba(255,255,255,0.03);
      border-radius: 10px;
      padding: 4px;
      gap: 4px;
    }

    .mode-tab {
      flex: 1;
      padding: 10px 8px;
      border: none;
      border-radius: 8px;
      background: transparent;
      color: var(--text-dim);
      font-size: 0.8rem;
      cursor: pointer;
      transition: all 0.3s;
    }

    .mode-tab.active {
      background: var(--accent);
      color: white;
      font-weight: 600;
    }

    /* ---- 控制台日志 ---- */
    .console {
      background: #0a0a14;
      border: 1px solid var(--border);
      border-radius: 12px;
      padding: 16px;
      font-family: 'Courier New', monospace;
      font-size: 0.78rem;
      color: #89ddff;
      height: 160px;
      overflow-y: auto;
      line-height: 1.6;
      margin-top: 8px;
    }

    .console::-webkit-scrollbar { width: 4px; }
    .console::-webkit-scrollbar-thumb { background: var(--border); border-radius: 2px; }

    .log-line { margin-bottom: 2px; }
    .log-time { color: var(--text-dim); margin-right: 8px; }
    .log-info { color: #89ddff; }
    .log-warn { color: var(--orange); }
    .log-ok { color: var(--green); }

    /* ---- 页脚 ---- */
    .footer {
      text-align: center;
      color: var(--text-dim);
      font-size: 0.75rem;
      margin-top: 10px;
      padding: 10px;
    }

    /* 通知提示 */
    .toast {
      position: fixed;
      top: 20px;
      right: 20px;
      background: var(--card);
      border: 1px solid var(--border);
      border-left: 4px solid var(--accent);
      padding: 12px 20px;
      border-radius: 10px;
      font-size: 0.85rem;
      z-index: 999;
      transform: translateX(120%);
      transition: transform 0.3s ease;
      box-shadow: 0 4px 20px var(--shadow);
    }

    .toast.show { transform: translateX(0); }
  </style>
</head>
<body>
  <div class="toast" id="toast"></div>

  <div class="container">
    <!-- 标题栏 -->
    <div class="header">
      <h1>ESP32 Web 控制面板</h1>
      <div class="status-bar">
        <div class="status-item">
          <div class="status-dot"></div>
          <span>在线</span>
        </div>
        <div class="status-item">📡 IP: <span id="ipAddr">--</span></div>
        <div class="status-item">🕐 运行: <span id="uptime">--</span></div>
        <div class="status-item">🧠 内存: <span id="memFree">--</span></div>
      </div>
    </div>

    <!-- 卡片区域 -->
    <div class="grid">

      <!-- LED 控制 -->
      <div class="card">
        <div class="card-title"><span class="icon">💡</span>LED 控制</div>
        <div class="led-display on" id="ledDisplay"><span>💡</span></div>
        <button class="toggle-btn on" id="toggleBtn" onclick="toggleLed()">关闭 LED</button>
        <div class="slider-wrap">
          <div class="slider-label">
            <span>亮度</span>
            <span id="brightnessVal">50%</span>
          </div>
          <input type="range" id="brightnessSlider" min="0" max="255" value="128"
                 oninput="setBrightness(this.value)">
        </div>
      </div>

      <!-- 温湿度 -->
      <div class="card">
        <div class="card-title"><span class="icon">🌡️</span>环境监测</div>
        <div class="sensor-grid">
          <div class="sensor-box">
            <div class="sensor-icon">🌡️</div>
            <div class="sensor-value temp" id="tempVal">--</div>
            <div class="sensor-label">温度 (°C)</div>
          </div>
          <div class="sensor-box">
            <div class="sensor-icon">💧</div>
            <div class="sensor-value humidity" id="humVal">--</div>
            <div class="sensor-label">湿度 (%)</div>
          </div>
        </div>
        <div style="margin-top:16px; display:flex; justify-content:space-between; align-items:center;">
          <div class="wifi-bars" id="wifiBars">
            <div class="wifi-bar"></div>
            <div class="wifi-bar"></div>
            <div class="wifi-bar"></div>
            <div class="wifi-bar"></div>
          </div>
          <span style="font-size:0.8rem; color:var(--text-dim);">Wi-Fi 信号</span>
        </div>
      </div>

      <!-- 系统状态 -->
      <div class="card">
        <div class="card-title"><span class="icon">📊</span>系统状态</div>
        <div class="stat-grid">
          <div class="stat-box">
            <div class="stat-value" id="chipModel">ESP32</div>
            <div class="stat-label">芯片型号</div>
          </div>
          <div class="stat-box">
            <div class="stat-value" id="cpuFreq">240</div>
            <div class="stat-label">CPU 频率 (MHz)</div>
          </div>
          <div class="stat-box">
            <div class="stat-value" id="flashSize">4</div>
            <div class="stat-label">Flash (MB)</div>
          </div>
          <div class="stat-box">
            <div class="stat-value" id="heapFree">--</div>
            <div class="stat-label">可用堆 (KB)</div>
          </div>
        </div>
      </div>

      <!-- 快捷指令 -->
      <div class="card">
        <div class="card-title"><span class="icon">⚡</span>快捷指令</div>
        <div class="quick-grid">
          <button class="quick-btn" onclick="quickCmd('blink-fast')">
            <span class="qi">⚡</span>快闪
          </button>
          <button class="quick-btn" onclick="quickCmd('blink-slow')">
            <span class="qi">🌙</span>慢闪
          </button>
          <button class="quick-btn" onclick="quickCmd('breath')">
            <span class="qi">🌬️</span>呼吸灯
          </button>
          <button class="quick-btn" onclick="quickCmd('rainbow')">
            <span class="qi">🌈</span>彩虹渐变
          </button>
          <button class="quick-btn" onclick="quickCmd('SOS')">
            <span class="qi">🆘</span>SOS 信号
          </button>
          <button class="quick-btn" onclick="quickCmd('reset')">
            <span class="qi">🔄</span>重启板子
          </button>
        </div>
      </div>

      <!-- 模式选择 -->
      <div class="card">
        <div class="card-title"><span class="icon">🎛️</span>工作模式</div>
        <div class="mode-tabs">
          <button class="mode-tab active" id="mode-auto" onclick="setMode('auto')">自动</button>
          <button class="mode-tab" id="mode-manual" onclick="setMode('manual')">手动</button>
          <button class="mode-tab" id="mode-sensor" onclick="setMode('sensor')">传感器</button>
        </div>
        <div class="console" id="consoleLog">
          <div class="log-line"><span class="log-time">[--:--:--]</span><span class="log-ok">系统就绪，等待指令...</span></div>
        </div>
      </div>

    </div>

    <div class="footer">ESP32 Web Dashboard &bull; Powered by AsyncWebServer</div>
  </div>

  <script>
    // ==================== 全局状态 ====================
    let ledOn = true;
    let brightness = 128;
    let currentMode = 'auto';
    let ws = null;

    // ==================== 初始化 ====================
    window.addEventListener('load', () => {
      fetch('/api/status').then(r => r.json()).then(data => {
        document.getElementById('ipAddr').textContent = data.ip;
        document.getElementById('chipModel').textContent = data.chip;
        document.getElementById('cpuFreq').textContent = data.freq;
        document.getElementById('flashSize').textContent = Math.round(data.flash / 1024 / 1024);
        updateWifiBars(data.rssi);
      });

      fetch('/api/led').then(r => r.json()).then(data => {
        ledOn = data.state;
        brightness = data.brightness;
        updateLedUI();
        document.getElementById('brightnessSlider').value = brightness;
        document.getElementById('brightnessVal').textContent = Math.round(brightness / 2.55) + '%';
      });

      // 定时刷新
      setInterval(fetchSensor, 5000);
      setInterval(updateUptime, 1000);
      setInterval(updateMem, 3000);
      fetchSensor();
      updateUptime();
      updateMem();
    });

    // ==================== API 调用 ====================
    function toggleLed() {
      ledOn = !ledOn;
      updateLedUI();
      fetch(`/api/led?state=${ledOn ? 'on' : 'off'}&brightness=${brightness}`);
      log(ledOn ? 'LED 已开启' : 'LED 已关闭', ledOn ? 'ok' : 'warn');
    }

    function setBrightness(val) {
      brightness = parseInt(val);
      document.getElementById('brightnessVal').textContent = Math.round(brightness / 2.55) + '%';
      fetch(`/api/led?state=${ledOn ? 'on' : 'off'}&brightness=${brightness}`);
    }

    function quickCmd(cmd) {
      fetch(`/api/cmd?action=${cmd}`).then(r => r.json()).then(data => {
        log(data.message, 'ok');
        showToast(data.message);
      });
    }

    function setMode(mode) {
      currentMode = mode;
      document.querySelectorAll('.mode-tab').forEach(t => t.classList.remove('active'));
      document.getElementById('mode-' + mode).classList.add('active');
      fetch(`/api/mode?mode=${mode}`);
      log('切换到模式: ' + mode, 'info');
    }

    function fetchSensor() {
      fetch('/api/sensor').then(r => r.json()).then(data => {
        document.getElementById('tempVal').textContent = data.temp;
        document.getElementById('humVal').textContent = data.hum;
        updateWifiBars(data.rssi);
      });
    }

    function updateUptime() {
      fetch('/api/uptime').then(r => r.text()).then(t => {
        document.getElementById('uptime').textContent = t;
      });
    }

    function updateMem() {
      fetch('/api/mem').then(r => r.json()).then(data => {
        document.getElementById('memFree').textContent = data.heap + ' KB';
        document.getElementById('heapFree').textContent = data.heap;
      });
    }

    function updateLedUI() {
      const display = document.getElementById('ledDisplay');
      const btn = document.getElementById('toggleBtn');
      if (ledOn) {
        display.className = 'led-display on';
        btn.className = 'toggle-btn on';
        btn.textContent = '关闭 LED';
      } else {
        display.className = 'led-display off';
        btn.className = 'toggle-btn off';
        btn.textContent = '开启 LED';
      }
    }

    function updateWifiBars(rssi) {
      const bars = document.querySelectorAll('.wifi-bar');
      let active = 0;
      if (rssi > -50) active = 4;
      else if (rssi > -60) active = 3;
      else if (rssi > -70) active = 2;
      else if (rssi > -80) active = 1;
      bars.forEach((b, i) => {
        b.classList.toggle('active', i < active);
      });
    }

    function log(msg, type = 'info') {
      const console = document.getElementById('consoleLog');
      const now = new Date();
      const time = [now.getHours(), now.getMinutes(), now.getSeconds()]
        .map(n => String(n).padStart(2, '0')).join(':');
      const line = document.createElement('div');
      line.className = 'log-line';
      line.innerHTML = `<span class="log-time">[${time}]</span><span class="log-${type}">${msg}</span>`;
      console.appendChild(line);
      console.scrollTop = console.scrollHeight;
    }

    function showToast(msg) {
      const toast = document.getElementById('toast');
      toast.textContent = msg;
      toast.classList.add('show');
      setTimeout(() => toast.classList.remove('show'), 2000);
    }
  </script>
</body>
</html>
)=====";

// ==================== 辅助函数 ====================

void setupPWM() {
  ledcAttach(LED_PIN, PWM_FREQ, PWM_RESOLUTION);
}

void setLedBrightness(int brightness) {
  ledcWrite(LED_PIN, brightness);
}

// ==================== Web 服务器路由 ====================

void setupRoutes() {
  // 主页
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", INDEX_HTML);
  });

  // LED 控制
  server.on("/api/led", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("state")) {
      String state = request->getParam("state")->value();
      if (state == "on") {
        ledState = true;
        setLedBrightness(ledBrightness);
      } else {
        ledState = false;
        setLedBrightness(0);
      }
    }
    if (request->hasParam("brightness")) {
      ledBrightness = request->getParam("brightness")->value().toInt();
      ledBrightness = constrain(ledBrightness, 0, 255);
      if (ledState) setLedBrightness(ledBrightness);
    }

    String response = "{\"state\":" + String(ledState ? "true" : "false") +
                       ",\"brightness\":" + String(ledBrightness) + "}";
    request->send(200, "application/json", response);
  });

  // 系统状态
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    int rssi = WiFi.RSSI();
    String response = "{"
      "\"ip\":\"" + WiFi.localIP().toString() + "\","
      "\"chip\":\"" + ESP.getChipModel() + "\","
      "\"freq\":" + String(ESP.getCpuFreqMHz()) + ","
      "\"flash\":" + String(ESP.getFlashChipSize()) + ","
      "\"rssi\":" + String(rssi) +
      "}";
    request->send(200, "application/json", response);
  });

  // 温湿度
  server.on("/api/sensor", HTTP_GET, [](AsyncWebServerRequest *request) {
#ifdef USE_DHT
    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    String response = "{"
      "\"temp\":\"" + String(isnan(temp) ? "--" : String(temp, 1)) + "\","
      "\"hum\":\"" + String(isnan(hum) ? "--" : String(hum, 0)) + "\","
      "\"rssi\":" + String(WiFi.RSSI()) +
      "}";
#else
    String response = "{"
      "\"temp\":\"--\","
      "\"hum\":\"--\","
      "\"rssi\":" + String(WiFi.RSSI()) +
      "}";
#endif
    request->send(200, "application/json", response);
  });

  // 内存
  server.on("/api/mem", HTTP_GET, [](AsyncWebServerRequest *request) {
    String response = "{"
      "\"heap\":" + String(ESP.getFreeHeap() / 1024) +
      "}";
    request->send(200, "application/json", response);
  });

  // 运行时间
  server.on("/api/uptime", HTTP_GET, [](AsyncWebServerRequest *request) {
    unsigned long secs = (millis() - startTime) / 1000;
    int h = secs / 3600;
    int m = (secs % 3600) / 60;
    int s = secs % 60;
    char buf[12];
    sprintf(buf, "%02d:%02d:%02d", h, m, s);
    request->send(200, "text/plain", buf);
  });

  // 快捷指令
  server.on("/api/cmd", HTTP_GET, [](AsyncWebServerRequest *request) {
    String action = request->hasParam("action") ? request->getParam("action")->value() : "";
    String msg = "未知指令";

    if (action == "blink-fast") {
      msg = "快闪模式 (100ms)";
    } else if (action == "blink-slow") {
      msg = "慢闪模式 (1000ms)";
    } else if (action == "breath") {
      msg = "呼吸灯模式";
    } else if (action == "rainbow") {
      msg = "彩虹渐变模式";
    } else if (action == "SOS") {
      msg = "SOS 求救信号";
    } else if (action == "reset") {
      msg = "正在重启...";
      delay(500);
      ESP.restart();
    }

    String response = "{\"action\":\"" + action + "\",\"message\":\"" + msg + "\"}";
    request->send(200, "application/json", response);
  });

  // 模式切换
  server.on("/api/mode", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("mode")) {
      currentMode = request->getParam("mode")->value();
    }
    request->send(200, "application/json", "{\"mode\":\"" + currentMode + "\"}");
  });

  // 404
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not Found");
  });
}

// ==================== 模式动画效果 ====================

void runLedEffect(String effect) {
  if (effect == "blink-fast") {
    for (int i = 0; i < 10; i++) {
      setLedBrightness(255);
      delay(100);
      setLedBrightness(0);
      delay(100);
    }
    setLedBrightness(ledState ? ledBrightness : 0);
  } else if (effect == "blink-slow") {
    for (int i = 0; i < 5; i++) {
      setLedBrightness(255);
      delay(1000);
      setLedBrightness(0);
      delay(1000);
    }
    setLedBrightness(ledState ? ledBrightness : 0);
  } else if (effect == "breath") {
    for (int cycle = 0; cycle < 3; cycle++) {
      for (int i = 0; i <= 255; i += 3) {
        setLedBrightness(i);
        delay(10);
      }
      for (int i = 255; i >= 0; i -= 3) {
        setLedBrightness(i);
        delay(10);
      }
    }
    setLedBrightness(ledState ? ledBrightness : 0);
  } else if (effect == "rainbow") {
    for (int cycle = 0; cycle < 2; cycle++) {
      for (int i = 0; i <= 255; i += 5) {
        setLedBrightness(i);
        delay(20);
      }
      for (int i = 255; i >= 0; i -= 5) {
        setLedBrightness(i);
        delay(20);
      }
    }
    setLedBrightness(ledState ? ledBrightness : 0);
  }
}

// ==================== MQTT 连接与发布 ====================

// MQTT 回调：接收远程指令
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  if (String(topic) == mqtt_topic_cmd) {
    if (msg.indexOf("\"led\":\"on\"") >= 0) {
      ledState = true;
      setLedBrightness(ledBrightness);
      Serial.println("MQTT 指令: LED 开");
    } else if (msg.indexOf("\"led\":\"off\"") >= 0) {
      ledState = false;
      setLedBrightness(0);
      Serial.println("MQTT 指令: LED 关");
    }
    // 立即上报 LED 状态
    String ledJson = "{\"led\":\"" + String(ledState ? "on" : "off") + "\",\"brightness\":" + String(ledBrightness) + "}";
    mqttClient.publish(mqtt_topic_led, ledJson.c_str());
  }
}

void connectMQTT() {
  if (mqttClient.connected()) return;

  Serial.print("正在连接 MQTT Broker...");
  String clientId = "ESP32_" + String(WiFi.macAddress());

  mqttClient.setServer(mqtt_broker, mqtt_port);
  mqttClient.setCallback(mqttCallback);

  if (mqttClient.connect(clientId.c_str())) {
    Serial.println(" 成功!");
    mqttClient.subscribe(mqtt_topic_cmd);
  } else {
    Serial.print(" 失败, rc=");
    Serial.println(mqttClient.state());
  }
}

// HTTP POST 上报到 PHP 虚拟主机
void httpReport(String json) {
  HTTPClient http;
  http.begin(HTTP_REPORT_URL);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(json);
  if (code > 0) {
    Serial.printf("HTTP 上报: %d\n", code);
  } else {
    Serial.printf("HTTP 失败: %s\n", http.errorToString(code).c_str());
  }
  http.end();
}

// 简易 URL 编码（用于 Server酱中文参数）
String urlEncode(String str) {
  String out = "";
  for (int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += c;
    } else if (c == ' ') {
      out += '+';
    } else {
      char hex[4];
      sprintf(hex, "%%%02X", (unsigned char)c);
      out += hex;
    }
  }
  return out;
}

// PushDeer 微信推送
void wxAlert(float temp, float hum) {
  unsigned long now = millis();
  if (lastWxAlert > 0 && (now - lastWxAlert) < WX_COOLDOWN) return;  // 冷却期
  lastWxAlert = now;

  HTTPClient http;
  http.begin("https://api2.pushdeer.com/message/push");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String title = "ESP32 温度告警 " + String(temp, 1) + "°C";
  String body = "温度: " + String(temp, 1) + "°C\\n湿度: " + String(hum, 0) + "%\\nWiFi信号: " + String(WiFi.RSSI()) + " dBm";
  String postData = "pushkey=" + String(PUSHDEER_KEY)
      + "&text=" + urlEncode(title)
      + "&desp=" + urlEncode(body)
      + "&type=text";

  int code = http.POST(postData);
  if (code > 0) {
    Serial.printf("PushDeer 推送成功 (%d)\n", code);
  } else {
    Serial.printf("PushDeer 推送失败: %s\n", http.errorToString(code).c_str());
  }
  http.end();
}

void publishSensorData() {
  String json = "{";

#ifdef USE_DHT
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  if (!isnan(temp) && !isnan(hum)) {
    char buf[8];
    dtostrf(temp, 4, 1, buf);
    mqttClient.publish(mqtt_topic_temp, buf);
    dtostrf(hum, 4, 1, buf);
    mqttClient.publish(mqtt_topic_hum, buf);

    json += "\"temp\":" + String(temp, 1) + ",";
    json += "\"hum\":" + String(hum, 0) + ",";
  } else {
    json += "\"temp\":null,";
    json += "\"hum\":null,";
  }
#else
  json += "\"temp\":null,";
  json += "\"hum\":null,";
#endif

  // 发布状态（WiFi 信号、运行时间始终可用）
  json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  json += "\"uptime\":" + String(millis() / 1000) + ",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\"";
  json += "}";
  mqttClient.publish(mqtt_topic_status, json.c_str());

	  // 微信推送（温度超阈值）
	  if (temp >= WX_ALERT_TEMP) wxAlert(temp, hum);
  // 上报 LED 状态
  String ledJson = "{\"led\":\"" + String(ledState ? "on" : "off") + "\",\"brightness\":" + String(ledBrightness) + "}";
  mqttClient.publish(mqtt_topic_led, ledJson.c_str());

  Serial.print("MQTT 发布: ");
  Serial.println(json);

  // HTTP POST 上报
  httpReport(json);
}

// ==================== 自动模式（传感器联动） ====================

void autoMode() {
#ifdef USE_DHT
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  if (!isnan(temp)) {
    if (temp > 30) {
      setLedBrightness(255);
    } else if (temp < 20) {
      setLedBrightness(50);
    }
  }
#endif
}

// ==================== OTA 无线升级 ====================

const char* ota_hostname = "esp32-sensor";
const char* ota_password = "12345678";  // OTA 升级密码

void setupOTA() {
  ArduinoOTA.setHostname(ota_hostname);
  ArduinoOTA.setPassword(ota_password);

  ArduinoOTA
    .onStart([]() {
      Serial.println("OTA 开始更新...");
    })
    .onEnd([]() {
      Serial.println("OTA 完成");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("OTA: %u%%\r", (progress * 100) / total);
    })
    .onError([](ota_error_t error) {
      Serial.printf("OTA 错误[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("认证失败");
      else if (error == OTA_BEGIN_ERROR) Serial.println("开始失败");
      else if (error == OTA_CONNECT_ERROR) Serial.println("连接失败");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("接收失败");
      else if (error == OTA_END_ERROR) Serial.println("结束失败");
    });

  ArduinoOTA.begin();
  Serial.print("OTA 就绪, 主机名: ");
  Serial.println(ota_hostname);
}

// ==================== 主程序 ====================

void setup() {
  Serial.begin(115200);
  delay(500);

  // LED 初始化
  setupPWM();
  setLedBrightness(ledBrightness);

#ifdef USE_DHT
  dht.begin();
#endif

  startTime = millis();

  // Wi-Fi 连接
  if (USE_AP_MODE) {
    WiFi.softAP(ap_ssid, ap_password);
    Serial.print("AP 模式, IP: ");
    Serial.println(WiFi.softAPIP());
  } else {
    WiFi.begin(ssid, password);
    Serial.print("正在连接 Wi-Fi...");
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n连接成功!");
      Serial.print("IP 地址: ");
      Serial.print(WiFi.localIP());
      Serial.print("  OTA: ");
      Serial.println(ota_hostname);
    } else {
      Serial.println("\nWi-Fi 连接失败，切换到 AP 模式");
      WiFi.softAP(ap_ssid, ap_password);
      Serial.print("AP IP: ");
      Serial.println(WiFi.softAPIP());
    }
  }

  setupOTA();
  setupRoutes();
  server.begin();
  Serial.println("Web 服务器已启动");
}

void loop() {
  ArduinoOTA.handle();

  // MQTT 保活 & 重连
  if (!mqttClient.connected()) {
    connectMQTT();
  }
  mqttClient.loop();

  // 定时发布传感器数据
  if (millis() - lastMqttPublish >= MQTT_INTERVAL) {
    lastMqttPublish = millis();
    publishSensorData();
  }

  if (currentMode == "auto") {
    autoMode();
  }
  delay(100);
}
