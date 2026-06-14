-- ESP32 环境监控 — 数据库表
-- 导入方式：phpMyAdmin 或 mysql -u root -p < install.sql

CREATE DATABASE IF NOT EXISTS esp32_monitor CHARACTER SET utf8 COLLATE utf8_general_ci;
USE esp32_monitor;

CREATE TABLE IF NOT EXISTS sensor_data (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  temperature FLOAT NULL COMMENT '温度 °C',
  humidity FLOAT NULL COMMENT '湿度 %',
  rssi INT NULL COMMENT 'WiFi 信号 / 电池百分比',
  uptime INT NULL COMMENT '运行秒数',
  ip VARCHAR(45) NULL COMMENT 'ESP32 IP',
  fw VARCHAR(32) NULL COMMENT 'ESP32 固件版本',
  lat DOUBLE NULL COMMENT 'GPS 纬度',
  lng DOUBLE NULL COMMENT 'GPS 经度',
  alt DOUBLE NULL COMMENT '海拔 m',
  spd DOUBLE NULL COMMENT '速度 km/h',
  sat INT NULL COMMENT '卫星数',
  `fix` INT NULL COMMENT '定位状态',
  cell_csq INT NULL COMMENT '蜂窝信号 0-31',
  cell_sim VARCHAR(16) NULL COMMENT 'SIM 卡状态',
  cell_net VARCHAR(16) NULL COMMENT '网络注册状态',
  cell_tech VARCHAR(16) NULL COMMENT '网络制式',
  ota_status VARCHAR(32) NULL COMMENT 'OTA 状态',
  recorded_at DATETIME NOT NULL COMMENT '记录时间',
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_recorded (recorded_at),
  INDEX idx_lat_lng (lat, lng),
  INDEX idx_gps_recorded (`fix`, recorded_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COMMENT='传感器历史数据';
