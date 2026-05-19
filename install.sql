-- ESP32 环境监控 — 数据库表
-- 导入方式：phpMyAdmin 或 mysql -u root -p < install.sql

CREATE DATABASE IF NOT EXISTS esp32_monitor CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE esp32_monitor;

CREATE TABLE IF NOT EXISTS sensor_data (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  temperature FLOAT NULL COMMENT '温度 °C',
  humidity FLOAT NULL COMMENT '湿度 %',
  rssi INT NULL COMMENT 'WiFi 信号',
  uptime INT NULL COMMENT '运行秒数',
  ip VARCHAR(45) NULL COMMENT 'ESP32 IP',
  recorded_at DATETIME NOT NULL COMMENT '记录时间',
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_recorded (recorded_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='传感器历史数据';
