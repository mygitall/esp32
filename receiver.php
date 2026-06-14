<?php
/**
 * ESP32 数据接收端点
 * 支持 GET 单条 + POST JSON 批量
 */

require_once __DIR__ . '/config.php';

header('Content-Type: application/json; charset=utf-8');
header('Access-Control-Allow-Origin: *');

$isGet = ($_SERVER['REQUEST_METHOD'] !== 'POST');
$rawBody = !$isGet ? json_decode(file_get_contents('php://input'), true) : null;
$params = $isGet ? $_GET : ($rawBody ?: []);

// GET batch 批量模式：?batch=lat1,lng1,alt1,spd1,sat1,bt1,mv1,csq1|lat2,lng2,...
if ($isGet && !empty($_GET['batch'])) {
    try {
        $db = getDB();
        $now = date('Y-m-d H:i:s');
        $stmt = $db->prepare(
            'INSERT INTO sensor_data (temperature, humidity, rssi, uptime, ip, lat, lng, alt, spd, sat, fix, cell_csq, recorded_at)
             VALUES (:temp, :hum, :rssi, :uptime, :ip, :lat, :lng, :alt, :spd, :sat, :fix, :csq, :ts)'
        );
        $count = 0;
        $parts = explode('|', $_GET['batch']);
        foreach ($parts as $part) {
            $f = explode(',', $part);
            if (count($f) < 8) continue;
            $lat = floatval($f[0]); $lng = floatval($f[1]);
            if (!isRealGpsPoint($lat, $lng, 1)) continue;
            $stmt->execute([
                'temp' => $f[6] ?? null, 'hum' => null,
                'rssi' => $f[5] ?? null, 'uptime' => null,
                'ip' => $_SERVER['REMOTE_ADDR'] ?? null,
                'lat' => $lat, 'lng' => $lng,
                'alt' => $f[2] ?? null, 'spd' => $f[3] ?? null,
                'sat' => $f[4] ?? null, 'fix' => 1,
                'csq' => $f[7] ?? null, 'ts' => $now,
            ]);
            $count++;
        }
        echo json_encode(['status' => 'ok', 'batch' => $count, 'ts' => $now]);
    } catch (Exception $e) {
        http_response_code(500);
        echo json_encode(['status' => 'error', 'message' => 'Internal error']);
    }
    exit;
}

// GET 无参数 = 返回统计
if ($isGet && empty($params['lat']) && empty($params['lng']) && empty($params['la'])) {
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

    // POST JSON 批量模式：[{la,lo,al,sp,sa,bt,mv,cs}, ...]
    if (is_array($params) && !empty($params[0]['la'])) {
        $stmt = $db->prepare(
            'INSERT INTO sensor_data (temperature, humidity, rssi, uptime, ip, lat, lng, alt, spd, sat, fix, cell_csq, recorded_at)
             VALUES (:temp, :hum, :rssi, :uptime, :ip, :lat, :lng, :alt, :spd, :sat, :fix, :csq, :ts)'
        );
        $count = 0;
        foreach ($params as $p) {
            $lat = floatval($p['la'] ?? 0);
            $lng = floatval($p['lo'] ?? 0);
            if (!isRealGpsPoint($lat, $lng, 1)) continue;
            $stmt->execute([
                'temp' => $p['mv'] ?? null, 'hum' => null,
                'rssi' => $p['bt'] ?? null, 'uptime' => null,
                'ip' => $_SERVER['REMOTE_ADDR'] ?? null,
                'lat' => $lat, 'lng' => $lng,
                'alt' => $p['al'] ?? null, 'spd' => $p['sp'] ?? null,
                'sat' => $p['sa'] ?? null, 'fix' => 1,
                'csq' => $p['cs'] ?? null, 'ts' => $now,
            ]);
            $count++;
        }
        echo json_encode(['status' => 'ok', 'batch' => $count, 'ts' => $now]);
        exit;
    }

    // 单条模式（GET）
    $lat = floatval($params['lat'] ?? 0);
    $lng = floatval($params['lng'] ?? 0);
    $fix = intval($params['fix'] ?? 0);
    $hasGps = isRealGpsPoint($lat, $lng, $fix);
    $stmt = $db->prepare(
        'INSERT INTO sensor_data (temperature, humidity, rssi, uptime, ip, fw, lat, lng, alt, spd, sat, fix, cell_csq, cell_sim, cell_net, cell_tech, ota_status, recorded_at)
         VALUES (:temp, :hum, :rssi, :uptime, :ip, :fw, :lat, :lng, :alt, :spd, :sat, :fix, :csq, :sim, :net, :tech, :ota, :ts)'
    );
    $stmt->execute([
        'temp'   => $params['mv'] ?? $params['temp'] ?? null,
        'hum'    => $params['hum'] ?? null,
        'rssi'   => $params['rssi'] ?? null,
        'uptime' => $params['uptime'] ?? null,
        'ip'     => $_SERVER['REMOTE_ADDR'] ?? null,
        'fw'     => isset($params['fw']) ? substr((string)$params['fw'], 0, 32) : null,
        'lat'    => $hasGps ? $lat : null,
        'lng'    => $hasGps ? $lng : null,
        'alt'    => $hasGps ? ($params['alt'] ?? null) : null,
        'spd'    => $hasGps ? ($params['spd'] ?? null) : null,
        'sat'    => $hasGps ? ($params['sat'] ?? null) : null,
        'fix'    => $hasGps ? 1 : 0,
        'csq'    => $params['csq'] ?? null,
        'sim'    => $params['sim'] ?? null,
        'net'    => $params['net'] ?? null,
        'tech'   => $params['tech'] ?? null,
        'ota'    => isset($params['ota']) ? substr((string)$params['ota'], 0, 32) : null,
        'ts'     => $now,
    ]);
    echo json_encode(['status' => 'ok', 'ts' => $now]);
} catch (Exception $e) {
    http_response_code(500);
    error_log('receiver: ' . $e->getMessage());
    echo json_encode(['status' => 'error', 'message' => 'Internal error']);
}

function isRealGpsPoint(float $lat, float $lng, int $fix): bool {
    return $fix === 1
        && !($lat === 0.0 && $lng === 0.0)
        && $lat >= 18 && $lat <= 54
        && $lng >= 73 && $lng <= 136;
}
