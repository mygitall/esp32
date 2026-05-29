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
#define DHT_PIN 14
#define DHT_TYPE DHT11

// ATGM336H GPS（UART2: GPIO16=RX2, GPIO17=TX2, 9600）
#define USE_GPS
#define GPS_BAUD 9600
#define GPS_RX 16
#define GPS_TX 17
HardwareSerial gpsSerial(2);
float gpsLat = 0, gpsLng = 0, gpsAlt = 0, gpsSpd = 0;
int gpsSat = 0, gpsFix = 0;
const unsigned long GPS_INTERVAL = 5000; // 每 5 秒更新一次 GPS
unsigned long lastGpsPublish = 0;

// Air780EX 4G 网络（UART1: GPIO4=RX, GPIO5=TX, PWRKEY=GPIO13）
#define USE_AIR780EX
#define AIR_BAUD 115200
#define AIR_RX 4
#define AIR_TX 5
#define AIR_PWRKEY 13
HardwareSerial airSerial(1);
bool airAtReady = false;

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
const char* mqtt_topic_gps  = "esp32/gps";
const char* mqtt_topic_cellular = "esp32/cellular";
const unsigned long MQTT_INTERVAL = 5000;  // 每 5 秒发布一次
const unsigned long CELLULAR_INTERVAL = 30000;  // 每 30 秒上报蜂窝状态
unsigned long lastCellularPublish = 0;

// HTTP POST 上报（存入 PHP 虚拟主机 MySQL）
const char* HTTP_REPORT_URL = "https://www.sseeee.com/esp32/mmq/receiver.php";
const char* CMD_POLL_URL = "https://www.sseeee.com/esp32/mmq/cmd_api.php?esp=1";

// Air780EX 4G 上报配置（联通）
const char* CELLULAR_APN = "UNINET";
const char* CELLULAR_REPORT_URL = "http://www.sseeee.com/esp32/mmq/receiver.php";
bool cellularReady = false;
unsigned long lastCellularSetup = 0;

// PushDeer 推送配置（一个 Key 推送到所有设备）
const char* PUSHDEER_KEY = "PDU41451T5iKoPmpeiumcfCkvMOYBMnFsN2NGEG7z";
const float WX_ALERT_TEMP = 30.0;     // 超过此温度触发推送
const unsigned long WX_COOLDOWN = 60000;  // 1 分钟内不重复
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
#ifdef USE_GPS
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

#ifdef USE_GPS
void readGPS();
#endif

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

// ==================== Air780EX 4G HTTP 上报 ====================

#ifdef USE_AIR780EX
bool probeAir780At(uint32_t baud);

String airRead(unsigned long timeoutMs) {
  String resp = "";
  unsigned long until = millis() + timeoutMs;
  while (millis() < until) {
    while (airSerial.available()) {
      resp += (char)airSerial.read();
    }
    delay(10);
  }
  return resp;
}

String airReadUntil(unsigned long timeoutMs, String marker1, String marker2 = "", String marker3 = "") {
  String resp = "";
  unsigned long until = millis() + timeoutMs;
  while (millis() < until) {
    while (airSerial.available()) {
      resp += (char)airSerial.read();
      if ((marker1.length() && resp.indexOf(marker1) >= 0) ||
          (marker2.length() && resp.indexOf(marker2) >= 0) ||
          (marker3.length() && resp.indexOf(marker3) >= 0) ||
          resp.indexOf("ERROR") >= 0) {
        return resp;
      }
    }
    delay(10);
  }
  return resp;
}

String airCommand(String cmd, unsigned long timeoutMs = 1000, String waitFor = "OK") {
  while (airSerial.available()) airSerial.read();
  airSerial.print(cmd);
  airSerial.print("\r\n");
  String resp = airReadUntil(timeoutMs, waitFor);
  Serial.print("Air780EX <= ");
  Serial.println(cmd);
  Serial.print(resp);
  return resp;
}

bool airCommandOk(String cmd, unsigned long timeoutMs = 1000) {
  String resp = airCommand(cmd, timeoutMs);
  return resp.indexOf("OK") >= 0;
}

bool setupCellular() {
  if (!airAtReady) {
    airAtReady = probeAir780At(115200) || probeAir780At(9600);
    if (!airAtReady) {
      Serial.println("Air780EX 4G 不可用：GPIO4/GPIO5 没收到 AT 响应");
      return false;
    }
  }

  unsigned long now = millis();
  if (cellularReady) return true;
  if (lastCellularSetup > 0 && now - lastCellularSetup < 60000) return false;
  lastCellularSetup = now;

  Serial.println("初始化 Air780EX 4G 网络...");
  if (!airCommandOk("AT", 1000)) return false;

  String sim = airCommand("AT+CPIN?", 1500);
  if (sim.indexOf("READY") < 0) {
    Serial.println("SIM 卡未就绪");
    return false;
  }

  airCommand("AT+CGATT?", 2000);
  airCommandOk("AT+SAPBR=0,1", 3000); // 已激活时可能返回错误，忽略
  if (!airCommandOk("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"", 1500)) return false;
  if (!airCommandOk(String("AT+SAPBR=3,1,\"APN\",\"") + CELLULAR_APN + "\"", 1500)) return false;
  if (!airCommandOk("AT+SAPBR=1,1", 10000)) return false;

  String ip = airCommand("AT+SAPBR=2,1", 3000);
  cellularReady = ip.indexOf("+SAPBR: 1,1") >= 0;
  Serial.println(cellularReady ? "Air780EX 4G 网络已就绪" : "Air780EX 4G 网络未获取到 IP");
  return cellularReady;
}

bool cellularHttpReport(String json) {
  if (!setupCellular()) return false;

  Serial.println("使用 Air780EX 4G 上报...");
  airCommand("AT+HTTPTERM", 1000); // 上次未释放时先清理，失败忽略
  if (!airCommandOk("AT+HTTPINIT", 3000)) {
    cellularReady = false;
    return false;
  }
  if (!airCommandOk("AT+HTTPPARA=\"CID\",1", 1500)) return false;
  airCommandOk("AT+HTTPSSL=0", 1500);
  if (!airCommandOk(String("AT+HTTPPARA=\"URL\",\"") + CELLULAR_REPORT_URL + "\"", 3000)) return false;
  if (!airCommandOk("AT+HTTPPARA=\"CONTENT\",\"application/json\"", 1500)) return false;

  while (airSerial.available()) airSerial.read();
  airSerial.print("AT+HTTPDATA=");
  airSerial.print(json.length());
  airSerial.print(",10000\r\n");
  String dataPrompt = airReadUntil(5000, "DOWNLOAD", ">");
  Serial.print(dataPrompt);
  if (dataPrompt.indexOf("DOWNLOAD") < 0 && dataPrompt.indexOf(">") < 0) {
    Serial.println("Air780EX 未进入 HTTPDATA 下载模式");
    airCommand("AT+HTTPTERM", 1000);
    return false;
  }

  airSerial.print(json);
  String dataResp = airReadUntil(12000, "OK");
  Serial.print(dataResp);
  if (dataResp.indexOf("OK") < 0) {
    airCommand("AT+HTTPTERM", 1000);
    return false;
  }

  String action = airCommand("AT+HTTPACTION=1", 15000, "+HTTPACTION:");
  action += airRead(500);
  int p = action.indexOf("+HTTPACTION:");
  bool ok = false;
  if (p >= 0) {
    int comma1 = action.indexOf(',', p);
    int comma2 = action.indexOf(',', comma1 + 1);
    int status = action.substring(comma1 + 1, comma2).toInt();
    ok = status >= 200 && status < 300;
    Serial.printf("Air780EX HTTP 状态: %d\n", status);
  }
  airCommand("AT+HTTPREAD", 3000);
  airCommand("AT+HTTPTERM", 1000);
  if (!ok) cellularReady = false;
  return ok;
}

void publishCellularStatus() {
  if (!airAtReady || !mqttClient.connected()) return;

  String cpin = airCommand("AT+CPIN?", 1500);
  bool simReady = cpin.indexOf("READY") >= 0;

  String csqResp = airCommand("AT+CSQ", 1500);
  int csqVal = 99;
  int p = csqResp.indexOf("+CSQ:");
  if (p >= 0) csqVal = csqResp.substring(p + 5).toInt();

  String creg = airCommand("AT+CREG?", 1500);
  int netStat = 0;
  p = creg.indexOf("+CREG:");
  if (p >= 0) {
    int comma = creg.indexOf(",", p);
    if (comma >= 0) netStat = creg.substring(comma + 1).toInt();
  }

  String cpsi = airCommand("AT+CPSI?", 2500);
  String netTech = "";
  p = cpsi.indexOf("+CPSI:");
  if (p >= 0) {
    int comma = cpsi.indexOf(",", p + 6);
    if (comma >= 0) netTech = cpsi.substring(p + 6, comma);
    netTech.trim();
  }

  String json = "{";
  json += "\"sim\":\"" + String(simReady ? "ready" : "error") + "\",";
  json += "\"csq\":" + String(csqVal) + ",";
  json += "\"net\":\"" + String(netStat == 1 || netStat == 5 ? "registered" : netStat == 2 ? "searching" : netStat == 3 ? "denied" : "unknown") + "\",";
  json += "\"tech\":\"" + netTech + "\"";
  json += "}";

  mqttClient.publish(mqtt_topic_cellular, json.c_str());
  Serial.print("Cellular: ");
  Serial.println(json);
}
#endif

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
  if (WiFi.status() != WL_CONNECTED) return;
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
  if (WiFi.status() != WL_CONNECTED) {
#ifdef USE_AIR780EX
    if (cellularHttpReport(json)) {
      Serial.println("Air780EX 4G 上报成功");
    } else {
      Serial.println("Air780EX 4G 上报失败");
    }
#else
    Serial.println("HTTP 跳过：Wi-Fi 未连接");
#endif
    return;
  }

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

// 轮询远程命令（小程序→PHP→ESP32）
void pollCmd() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(CMD_POLL_URL);
  int code = http.GET();
  if (code == 200) {
    String body = http.getString();
    if (body.indexOf("\"led_on\"") >= 0) {
      ledState = true; setLedBrightness(ledBrightness);
      Serial.println("远程命令: LED 开");
    } else if (body.indexOf("\"led_off\"") >= 0) {
      ledState = false; setLedBrightness(0);
      Serial.println("远程命令: LED 关");
    }
  }
  http.end();
}

// PushDeer 推送
void wxAlert(float temp, float hum) {
  if (WiFi.status() != WL_CONNECTED) return;

  unsigned long now = millis();
  if (lastWxAlert > 0 && (now - lastWxAlert) < WX_COOLDOWN) return;
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
  if (code > 0) Serial.printf("PushDeer OK(%d)\n", code);
  else Serial.printf("PushDeer 失败: %s\n", http.errorToString(code).c_str());
  http.end();
}

void publishSensorData() {
#ifdef USE_GPS
  readGPS();
#endif

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
#ifdef USE_GPS
  json += ",\"lat\":" + String(gpsLat, 6) + ",\"lng\":" + String(gpsLng, 6);
  json += ",\"alt\":" + String(gpsAlt, 1) + ",\"spd\":" + String(gpsSpd, 1);
  json += ",\"sat\":" + String(gpsSat) + ",\"fix\":" + String(gpsFix);
#endif
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
  pollCmd();
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

// ==================== ATGM336H GPS 定位 ====================

#ifdef USE_GPS

float nmeaCoordToDecimal(String raw, String hemi) {
  raw.trim();
  hemi.trim();
  if (raw.length() < 4) return 0;

  int dot = raw.indexOf('.');
  int degLen = (dot > 4) ? dot - 2 : 2;
  float degrees = raw.substring(0, degLen).toFloat();
  float minutes = raw.substring(degLen).toFloat();
  float value = degrees + minutes / 60.0;
  if (hemi == "S" || hemi == "W") value = -value;
  return value;
}

int splitCsv(String data, String fields[], int maxFields) {
  int count = 0;
  int start = 0;
  data.trim();
  for (int i = 0; i <= data.length() && count < maxFields; i++) {
    if (i == data.length() || data[i] == ',') {
      fields[count++] = data.substring(start, i);
      start = i + 1;
    }
  }
  return count;
}

bool parseCgnsinf(String line) {
  int idx = line.indexOf("+CGNSINF:");
  if (idx < 0) return false;

  String data = line.substring(idx + 9);
  data.trim();
  int end = data.indexOf('\n');
  if (end >= 0) data = data.substring(0, end);

  String fields[20];
  int f = splitCsv(data, fields, 20);
  if (f < 7) return false;

  // Air780EX/Luat CGNSINF:
  // run_status,fix_status,utc,lat,lng,alt,speed,course,...
  int fix = fields[1].toInt();
  float lat = fields[3].toFloat();
  float lng = fields[4].toFloat();
  if (fix <= 0 || (lat == 0 && lng == 0)) return false;

  gpsFix = fix;
  gpsLat = lat;
  gpsLng = lng;
  gpsAlt = fields[5].toFloat();
  gpsSpd = fields[6].toFloat();
  if (f > 15) gpsSat = fields[15].toInt();
  else if (f > 14) gpsSat = fields[14].toInt();
  return true;
}

bool parseNmeaSentence(String line) {
  line.trim();
  if (!line.startsWith("$")) return false;

  int star = line.indexOf('*');
  if (star > 0) line = line.substring(0, star);

  String fields[20];
  int f = splitCsv(line, fields, 20);
  if (f < 2) return false;

  String type = fields[0];
  if (type.endsWith("GGA") && f >= 10) {
    int fix = fields[6].toInt();
    float lat = nmeaCoordToDecimal(fields[2], fields[3]);
    float lng = nmeaCoordToDecimal(fields[4], fields[5]);
    if (fix <= 0 || (lat == 0 && lng == 0)) return false;

    gpsFix = fix;
    gpsLat = lat;
    gpsLng = lng;
    gpsSat = fields[7].toInt();
    gpsAlt = fields[9].toFloat();
    return true;
  }

  if ((type.endsWith("RMC") || type.endsWith("GNRMC")) && f >= 9) {
    if (fields[2] != "A") return false;
    float lat = nmeaCoordToDecimal(fields[3], fields[4]);
    float lng = nmeaCoordToDecimal(fields[5], fields[6]);
    if (lat == 0 && lng == 0) return false;

    gpsFix = 1;
    gpsLat = lat;
    gpsLng = lng;
    gpsSpd = fields[7].toFloat() * 1.852; // knots -> km/h
    return true;
  }

  return false;
}

bool parseGpsBuffer(String raw) {
  bool ok = false;

  int start = 0;
  while (start < raw.length()) {
    int end = raw.indexOf('\n', start);
    if (end < 0) end = raw.length();
    String line = raw.substring(start, end);
    if (parseNmeaSentence(line)) ok = true;
    start = end + 1;
  }

  return ok;
}

String readGpsSerial(unsigned long durationMs) {
  String raw = "";
  unsigned long until = millis() + durationMs;
  while (millis() < until) {
    while (gpsSerial.available()) {
      raw += (char)gpsSerial.read();
    }
    delay(10);
  }
  return raw;
}

void initGPS() {
  Serial.println("\n====== ATGM336H GPS 初始化 ======");
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);
  delay(500);
  readGpsSerial(500);
  Serial.println("GPS NMEA 串口已启动（首次定位可能需要 30-120s）\n");
}

void wifiLocate();  // 前向声明

void readGPS() {
  String raw = readGpsSerial(1200);

  if (!parseGpsBuffer(raw)) {
    Serial.print("GPS: 无响应 raw=[");
    Serial.print(raw.length() ? raw : "(空)");
    Serial.println("]");
    return;
  }

  // GPS 坐标为空时用 WiFi 定位补充
  if (gpsLat == 0 && gpsLng == 0) {
    wifiLocate();
    if (gpsLat == 0 && gpsLng == 0) {
      gpsFix = 0;  // 两边都没定位到
      Serial.println("GPS+WiFi: 均未定位");
      return;
    }
  }

  Serial.printf("GPS: %.5f,%.5f alt=%.0fm spd=%.1f sat=%d\n",
    gpsLat, gpsLng, gpsAlt, gpsSpd, gpsSat);
}

void wifiLocate() {
  HTTPClient http;
  http.begin("http://ip-api.com/json/?fields=lat,lon");
  http.setTimeout(5000);
  int code = http.GET();
  if (code == 200) {
    String body = http.getString();
    int li = body.indexOf("\"lat\":");
    int ni = body.indexOf("\"lon\":");
    if (li > 0 && ni > 0) {
      gpsLat = body.substring(li + 6, body.indexOf(",", li)).toFloat();
      gpsLng = body.substring(ni + 6, body.indexOf("}", ni)).toFloat();
      gpsFix = 1;
      Serial.printf("IP定位: %.4f, %.4f\n", gpsLat, gpsLng);
    }
  }
  http.end();
}

#endif

// ==================== Air780EX 初始化 ====================

#ifdef USE_AIR780EX
bool probeAir780At(uint32_t baud) {
  airSerial.updateBaudRate(baud);
  delay(300);
  airRead(300);
  for (int i = 0; i < 3; i++) {
    airSerial.print("AT\r\n");
    String resp = airRead(700);
    if (resp.indexOf("OK") >= 0) {
      Serial.print("Air780EX AT 通信 OK, baud=");
      Serial.println(baud);
      return true;
    }
  }
  return false;
}

void initAir780EX() {
  Serial.println("\n====== Air780EX 4G 初始化 ======");
  pinMode(AIR_PWRKEY, OUTPUT);
  digitalWrite(AIR_PWRKEY, LOW);
  delay(2000);
  digitalWrite(AIR_PWRKEY, HIGH);
  Serial.println("Air780EX PWRKEY 已触发");

  airSerial.begin(AIR_BAUD, SERIAL_8N1, AIR_RX, AIR_TX);
  delay(8000);
  airAtReady = probeAir780At(115200) || probeAir780At(9600);
  if (!airAtReady) {
    Serial.println("Air780EX AT 无响应，请检查 GPIO4/GPIO5 接线、GND、模块开机状态");
  }
}
#endif

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

#ifdef USE_GPS
  initGPS();
#endif

#ifdef USE_AIR780EX
  initAir780EX();
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

  // 定时发布传感器数据（5s）
  if (millis() - lastMqttPublish >= MQTT_INTERVAL) {
    lastMqttPublish = millis();
    publishSensorData();
  }

#ifdef USE_GPS
  // GPS 定时发布（坐标在 publishSensorData 中每 5 秒更新）
  if (millis() - lastGpsPublish >= GPS_INTERVAL) {
    lastGpsPublish = millis();
    if (gpsFix > 0 && mqttClient.connected()) {
      char buf[100];
      snprintf(buf, sizeof(buf),
        "{\"lat\":%.6f,\"lng\":%.6f,\"alt\":%.1f,\"spd\":%.1f,\"sat\":%d,\"fix\":%d}",
        gpsLat, gpsLng, gpsAlt, gpsSpd, gpsSat, gpsFix);
      mqttClient.publish(mqtt_topic_gps, buf);
    }
  }
#endif

#ifdef USE_AIR780EX
  if (millis() - lastCellularPublish >= CELLULAR_INTERVAL) {
    lastCellularPublish = millis();
    publishCellularStatus();
  }
#endif

  if (currentMode == "auto") {
    autoMode();
  }
  delay(100);
}
