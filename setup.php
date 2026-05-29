<?php
/**
 * 一键建表 — 访问此文件自动创建数据库表
 * 用法：浏览器打开 https://www.sseeee.com/esp32/mmq/setup.php
 * 建完后建议删除此文件
 */

require_once __DIR__ . '/config.php';

header('Content-Type: application/json; charset=utf-8');

try {
    $db = getDB();
    $charset = defined('DB_FALLBACK_CHARSET') ? DB_FALLBACK_CHARSET : 'utf8';

    $sql = "CREATE TABLE IF NOT EXISTS sensor_data (
        id BIGINT AUTO_INCREMENT PRIMARY KEY,
        temperature FLOAT NULL COMMENT '温度 °C',
        humidity FLOAT NULL COMMENT '湿度 %',
        rssi INT NULL COMMENT 'WiFi 信号',
        uptime INT NULL COMMENT '运行秒数',
        ip VARCHAR(45) NULL COMMENT 'ESP32 IP',
        recorded_at DATETIME NOT NULL COMMENT '记录时间',
        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
        INDEX idx_recorded (recorded_at)
    ) ENGINE=InnoDB DEFAULT CHARSET={$charset} COMMENT='传感器历史数据'";

    $db->exec($sql);

    echo json_encode([
        'status' => 'ok',
        'message' => '数据库表 sensor_data 已就绪'
    ], JSON_UNESCAPED_UNICODE);

} catch (Exception $e) {
    http_response_code(500);
    echo json_encode([
        'status' => 'error',
        'message' => $e->getMessage()
    ], JSON_UNESCAPED_UNICODE);
}
