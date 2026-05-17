# ESP32 环境监控面板

ESP32 + DHT11 温湿度监控，MQTT 云端传输，手机 4G 远程查看 + LED 远程控制。

## 功能

- DHT11 温湿度采集，每 5 秒上报
- MQTT 上传到云端 Broker（broker.emqx.io）
- monitor.html 独立页面，手机浏览器直接打开查看
- LED 远程开关控制
- 本地 Web 仪表盘（AsyncWebServer）
- OTA 无线升级

## 文件说明

```
sketch_may17a/
├── sketch_may17a.ino    ← 主程序（WiFi / MQTT / Web / LED / OTA）
├── monitor.html          ← 手机监控页面
└── README.md
```

## 配置

在 `sketch_may17a.ino` 顶部修改：

```cpp
const char* ssid = "WIFI";         // WiFi 名称
const char* password = "999999999"; // WiFi 密码
```

## OTA 无线升级

### 使用方法

烧录一次后，ESP32 不需要再插 USB 数据线。

**Arduino IDE：**

1. ESP32 插充电头或充电宝供电
2. 电脑连同一个 WiFi
3. 工具 → 端口 → 选择网络端口 `esp32-sensor`
4. 点上传，输入密码 `12345678`

**Claude Code（命令行）：**

直接说「帮我 OTA 升级」，自动通过 WiFi 编译上传，全程不需要 USB 线。

### 原理

```
ESP32（插充电宝）──WiFi──路由器──WiFi──电脑（Arduino IDE / Claude Code）
                                     │
                                     └── OTA 无线烧录
```

核心条件：ESP32 和电脑在**同一个局域网**。ESP32 插哪供电都行。

## 手机远程查看

1. 打开 `monitor.html`（可部署到 PHP 虚拟主机或 GitHub Pages）
2. 页面自动连接云端 MQTT Broker
3. 4G / WiFi 都能查看温湿度和控制 LED

## 依赖库

- PubSubClient
- Async_TCP (ESP32Async)
- ESP_Async_WebServer (ESP32Async)
- DHT sensor library
- ArduinoOTA（ESP32 内置）
