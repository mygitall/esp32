# ESP32 GPS 追踪器 — 服务端

实时 GPS 追踪系统的服务端代码：数据接收、存储、查询、地图展示。

## 仓库说明

本项目拆分为两个独立仓库：

| 仓库 | 内容 | 本地路径 |
|------|------|---------|
| `esp32` | 服务端（PHP + HTML） | `~/Desktop/AI开发/esp32/` |
| `esp32-gps-tracker` | 固件（.ino 草图） | `~/esp32-gps-tracker/` |

## 拉取到本地

```bash
# 服务端仓库
git clone https://github.com/mygitall/esp32.git ~/Desktop/AI开发/esp32

# 固件仓库
git clone https://github.com/mygitall/esp32-gps-tracker.git ~/esp32-gps-tracker
```

## 文件说明

```
esp32/
├── receiver.php          ← 数据接收端点（GET 单条 + GET 批量 + POST JSON 批量）
├── api.php               ← 数据查询 API（?latest=1 最新点，?from=...&to=... 时间范围）
├── subscribe_daemon.php  ← MQTT 订阅守护进程（cron 模式每 1 分钟采集）
├── mqtt_client.php       ← 纯 PHP MQTT 3.1.1 客户端（零依赖）
├── config.php            ← 数据库连接 + MQTT 配置
├── map.html              ← GPS 轨迹地图（Leaflet + 高德底图 + MQTT/HTTP 实时更新）
├── setup.php             ← 数据库初始化脚本
├── upgrade_db.php        ← 数据库迁移脚本
└── install.sql           ← 初始 SQL 建表语句
```

## 部署到服务器

服务器文件路径: `/esp32/mmq/`

FTP 上传：
```bash
python3 -c "
import ftplib
ftp = ftplib.FTP()
ftp.connect('gao367888125.zfkwp02.guaixing.cn', 21, timeout=15)
ftp.login('gao367888125', 'c6f57i8j')
files = ['receiver.php','api.php','config.php','subscribe_daemon.php','mqtt_client.php','map.html']
for f in files:
    with open(f, 'rb') as fh:
        ftp.storbinary(f'STOR /esp32/mmq/{f}', fh)
    print(f'{f} uploaded')
ftp.quit()
"
```

## API 接口

### 数据查询 (api.php)

```bash
# 最新一条
curl https://www.sseeee.com/esp32/mmq/api.php?latest=1

# 按时间范围
curl "https://www.sseeee.com/esp32/mmq/api.php?from=2026-06-08+10:00:00&to=2026-06-08+11:00:00"

# 按天查询 + 按小时聚合
curl "https://www.sseeee.com/esp32/mmq/api.php?date=2026-06-08&format=hourly"
```

### 数据接收 (receiver.php)

```bash
# 批量 GET
curl "https://www.sseeee.com/esp32/mmq/receiver.php?batch=30.956,121.805,10,30,15,50,3875,31|30.957,121.806,11,32,15,50,3875,31"

# 批量 POST JSON
curl -X POST "https://www.sseeee.com/esp32/mmq/receiver.php" \
  -H "Content-Type: application/json" \
  -d '[{"la":30.956,"lo":121.805,"al":10,"sp":30,"sa":15,"bt":50,"mv":3875,"cs":31}]'
```

### 统计

```bash
curl https://www.sseeee.com/esp32/mmq/receiver.php
# → {"status":"ok","total_records":20664}
```

## 架构

```
ATGM336H GPS → ESP32 → Air780EX 4G → HTTP → receiver.php → MySQL → api.php → map.html
                                                                     ↘ subscribe_daemon.php (MQTT 备路)
```

- HTTP 批量上报每 10 秒一次（`?batch=` 格式）
- MQTT 守护进程作为备用数据路径（需外部 cron 每分钟触发）
- map.html 优先用 MQTT WebSocket 实时更新，降级到 HTTP 3 秒轮询
