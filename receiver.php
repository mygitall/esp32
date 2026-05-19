<?php
/**
 * ESP32 数据接收端点
 * ESP32 每 5 秒 HTTP POST 温湿度数据到这里
 *
 * URL: https://www.sseeee.com/esp32/mmq/receiver.php
 */

require_once __DIR__ . '/config.php';

header('Content-Type: application/json; charset=utf-8');
header('Access-Control-Allow-Origin: *');

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    // GET 请求返回状态（测试用）
    try {
        $db = getDB();
        $stmt = $db->query('SELECT COUNT(*) AS cnt FROM sensor_data');
        $row = $stmt->fetch();
        echo json_encode(['status' => 'ok', 'total_records' => (int)$row['cnt']]);
    } catch (Exception $e) {
        echo json_encode(['status' => 'error', 'message' => $e->getMessage()]);
    }
    exit;
}

// POST 请求：接收并存储数据
$input = json_decode(file_get_contents('php://input'), true);
if (!$input) {
    http_response_code(400);
    echo json_encode(['status' => 'error', 'message' => '无效 JSON']);
    exit;
}

try {
    $db = getDB();
    $stmt = $db->prepare(
        'INSERT INTO sensor_data (temperature, humidity, rssi, uptime, ip, recorded_at)
         VALUES (:temp, :hum, :rssi, :uptime, :ip, NOW())'
    );
    $stmt->execute([
        'temp'   => $input['temp'] ?? null,
        'hum'    => $input['hum'] ?? null,
        'rssi'   => $input['rssi'] ?? null,
        'uptime' => $input['uptime'] ?? null,
        'ip'     => $_SERVER['REMOTE_ADDR'] ?? null,
    ]);

    echo json_encode(['status' => 'ok', 'ts' => date('Y-m-d H:i:s')]);
} catch (Exception $e) {
    http_response_code(500);
    echo json_encode(['status' => 'error', 'message' => $e->getMessage()]);
}
