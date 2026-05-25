<?php
/**
 * 历史数据查询 API
 *
 * 参数:
 *   date   - 日期 (YYYY-MM-DD)，不传返回今天
 *   from   - 起始时间 (HH:MM)，可选
 *   to     - 结束时间 (HH:MM)，可选
 *   range  - 预设范围: today / yesterday / 7days / 30days
 *   format - points (默认，所有点) / hourly (按小时平均) / daily (按天平均)
 *
 * 示例:
 *   /mmq/api.php?date=2026-05-19&from=08:00&to=18:00
 *   /mmq/api.php?range=7days&format=hourly
 */

header('Content-Type: application/json; charset=utf-8');
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: GET');

require_once __DIR__ . '/config.php';

try {
    $db = getDB();

    // 逆地理编码代理（前端免 Key）
    if (($_GET['geo'] ?? '') === '1') {
        $lat = $_GET['lat'] ?? 0;
        $lng = $_GET['lng'] ?? 0;
        $url = "https://nominatim.openstreetmap.org/reverse?format=json&lat={$lat}&lon={$lng}&zoom=18&accept-language=zh";
        $ctx = stream_context_create(['http'=>['timeout'=>5,'header'=>"User-Agent: ESP32-GPS/1.0\r\n"]]);
        $resp = @file_get_contents($url, false, $ctx);
        if ($resp) {
            $data = json_decode($resp, true);
            $addr = $data['address'] ?? [];
            echo json_encode([
                'status'=>'ok',
                'road'=>$addr['road']??($addr['pedestrian']??''),
                'number'=>$addr['house_number']??'',
                'district'=>$addr['suburb']??($addr['district']??''),
                'display'=>$data['display_name']??''
            ], JSON_UNESCAPED_UNICODE);
        } else {
            echo json_encode(['status'=>'error','message'=>'API fail']);
        }
        exit;
    }

    // 最新一条：秒开缓存用
    if (($_GET['latest'] ?? '') === '1') {
        $stmt = $db->query('SELECT recorded_at AS ts, temperature AS temp, humidity AS hum, rssi, lat, lng, alt, spd, sat, fix FROM sensor_data ORDER BY id DESC LIMIT 1');
        $row = $stmt->fetch();
        echo json_encode(['status' => 'ok', 'latest' => $row ?: null], JSON_UNESCAPED_UNICODE);
        exit;
    }

    $params = parseParams();
    $data = queryData($db, $params);
    echo json_encode([
        'status' => 'ok',
        'count'  => count($data),
        'params' => $params,
        'data'   => $data
    ], JSON_UNESCAPED_UNICODE);

} catch (Exception $e) {
    http_response_code(500);
    echo json_encode(['status' => 'error', 'message' => $e->getMessage()]);
}

// ==================== 参数解析 ====================
function parseParams(): array {
    $range = $_GET['range'] ?? '';
    $date  = $_GET['date'] ?? '';
    $from  = $_GET['from'] ?? '';
    $to    = $_GET['to'] ?? '';
    $format = $_GET['format'] ?? 'points';

    switch ($range) {
        case 'today':
            $date = date('Y-m-d');
            break;
        case 'yesterday':
            $date = date('Y-m-d', strtotime('-1 day'));
            break;
        case '7days':
            $date = date('Y-m-d', strtotime('-7 day'));
            $to = date('Y-m-d 23:59:59');
            break;
        case '30days':
            $date = date('Y-m-d', strtotime('-30 day'));
            $to = date('Y-m-d 23:59:59');
            break;
    }

    // 构建 SQL 时间范围
    if (in_array($range, ['7days', '30days'])) {
        $fromDt = $date;
        $toDt = $to;
    } elseif ($date) {
        $fromDt = $date . ($from ? " {$from}:00" : ' 00:00:00');
        $toDt = $date . ($to ? " {$to}:00" : ' 23:59:59');
    } else {
        $fromDt = date('Y-m-d 00:00:00');
        $toDt = date('Y-m-d 23:59:59');
    }

    return ['from' => $fromDt, 'to' => $toDt, 'format' => $format];
}

// ==================== 查询 ====================
function queryData(PDO $db, array $params): array {
    $format = $params['format'];

    if ($format === 'hourly') {
        $sql = "SELECT DATE_FORMAT(recorded_at, '%Y-%m-%d %H:00') AS ts,
                       AVG(temperature) AS temp, AVG(humidity) AS hum,
                       AVG(rssi) AS rssi
                FROM sensor_data
                WHERE recorded_at BETWEEN :from AND :to
                GROUP BY ts ORDER BY ts ASC";
    } elseif ($format === 'daily') {
        $sql = "SELECT DATE(recorded_at) AS ts,
                       AVG(temperature) AS temp, AVG(humidity) AS hum,
                       AVG(rssi) AS rssi
                FROM sensor_data
                WHERE recorded_at BETWEEN :from AND :to
                GROUP BY ts ORDER BY ts ASC";
    } else {
        // points: 返回所有原始点
        $sql = "SELECT recorded_at AS ts, temperature AS temp, humidity AS hum, rssi, lat, lng, alt, spd, sat, fix
                FROM sensor_data
                WHERE recorded_at BETWEEN :from AND :to
                ORDER BY recorded_at ASC";
    }

    $stmt = $db->prepare($sql);
    $stmt->execute(['from' => $params['from'], 'to' => $params['to']]);
    return $stmt->fetchAll();
}
