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

$isGet = ($_SERVER['REQUEST_METHOD'] !== 'POST');
$params = $isGet ? $_GET : (json_decode(file_get_contents('php://input'), true) ?: []);

// GET 无参数 = 返回统计
if ($isGet && empty($params['lat']) && empty($params['lng'])) {
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

try {
    $db = getDB();
    $now = date('Y-m-d H:i:s');

    // 校验坐标合法性：拒绝 (0,0) 和超出中国范围的值
    $lat = floatval($params['lat'] ?? 0);
    $lng = floatval($params['lng'] ?? 0);
    if (($lat === 0.0 && $lng === 0.0) || $lat < 18 || $lat > 54 || $lng < 73 || $lng > 136) {
        echo json_encode(['status' => 'error', 'message' => 'invalid coordinates']);
        exit;
    }
    $stmt = $db->prepare(
        'INSERT INTO sensor_data (temperature, humidity, rssi, uptime, ip, lat, lng, alt, spd, sat, fix, cell_csq, cell_sim, cell_net, cell_tech, recorded_at)
         VALUES (:temp, :hum, :rssi, :uptime, :ip, :lat, :lng, :alt, :spd, :sat, :fix, :csq, :sim, :net, :tech, :ts)'
    );
    $stmt->execute([
        'temp'   => $params['temp'] ?? null,
        'hum'    => $params['hum'] ?? null,
        'rssi'   => $params['rssi'] ?? null,
        'uptime' => $params['uptime'] ?? null,
        'ip'     => $_SERVER['REMOTE_ADDR'] ?? null,
        'lat'    => $params['lat'] ?? null,
        'lng'    => $params['lng'] ?? null,
        'alt'    => $params['alt'] ?? null,
        'spd'    => $params['spd'] ?? null,
        'sat'    => $params['sat'] ?? null,
        'fix'    => $params['fix'] ?? null,
        'csq'    => $params['csq'] ?? null,
        'sim'    => $params['sim'] ?? null,
        'net'    => $params['net'] ?? null,
        'tech'   => $params['tech'] ?? null,
        'ts'     => $now,
    ]);
    echo json_encode(['status' => 'ok', 'ts' => $now]);
} catch (Exception $e) {
    http_response_code(500);
    echo json_encode(['status' => 'error', 'message' => $e->getMessage()]);
}
