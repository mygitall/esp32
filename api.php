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
 *   gps_only  - 1 只返回真实 GPS 点（fix=1 且坐标有效）
 *   min_dist  - 相邻点最小距离，单位米
 *   max_speed - 相邻点最大速度，单位 km/h
 *   min_sat   - 最小卫星数
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

    // 逆地理编码代理（带本地缓存，50m 范围内 24h 复用）
    if (($_GET['geo'] ?? '') === '1') {
        $lat = $_GET['lat'] ?? 0;
        $lng = $_GET['lng'] ?? 0;
        // 缓存键：坐标四舍五入到小数点后 4 位（~11m 网格），1小时过期
        $cacheKey = round($lat,4) . ',' . round($lng,4);
        $cacheDir = __DIR__ . '/cache';
        $cacheFile = $cacheDir . '/' . md5($cacheKey) . '.json';
        if (!is_dir($cacheDir)) mkdir($cacheDir, 0755, true);

        // 缓存命中且未过期（1 小时）
        if (file_exists($cacheFile) && (time() - filemtime($cacheFile)) < 3600) {
            echo file_get_contents($cacheFile);
            exit;
        }

        // 并行查两个免费地理编码服务，取最好的结果
        $road = ''; $num = ''; $district = ''; $city = ''; $state = ''; $display = '';
        $services = [
            ['url'=>'https://nominatim.openstreetmap.org/reverse?format=jsonv2&lat=%f&lon=%f&zoom=17&accept-language=zh&addressdetails=1','road_fields'=>['road','residential','service','pedestrian','footway','path','living_street','cycleway','secondary','tertiary','unclassified','track']],
        ];

        foreach ($services as $svc) {
            $url = sprintf($svc['url'], $lat, $lng);
            $ctx = stream_context_create(['http'=>['timeout'=>5,'header'=>"User-Agent: ESP32-GPS/1.0\r\n"]]);
            $resp = @file_get_contents($url, false, $ctx);
            if (!$resp) continue;
            $data = json_decode($resp, true);
            if (!$data) continue;
            $addr = $data['address'] ?? [];
            // 从地址字段中提取路名
            foreach ($svc['road_fields'] as $f) {
                if (!empty($addr[$f])) { $road = $addr[$f]; break; }
            }
            $num = $addr['house_number']??'';
            $district = $addr['district']??($addr['suburb']??($addr['county']??($addr['state_district']??'')));
            $city = $addr['city']??($addr['town']??($addr['village']??''));
            $state = $addr['state']??($addr['province']??($addr['region']??''));
            $display = $data['display_name']??'';
            // 拿到路名就跳出
            if ($road) break;
        }

        // display_name 兜底解析
        if(!$road&&!$district&&!$city&&!$state){
            $parts = array_map('trim', explode(',', $display));
            $c = count($parts);
            if($c>=1) $road = $parts[0];
            if(!$district && $c>=3) $district = $parts[$c-3];
            if(!$city && $c>=4) $city = $parts[$c-4];
            if(!$state && $c>=4) $state = end($parts) === '中国' ? $parts[$c-2] : ($parts[$c-1]??'');
        }

        $out = json_encode([
            'status'=>'ok',
            'road'=>$road, 'number'=>$num,
            'district'=>$district, 'city'=>$city, 'state'=>$state,
            'display'=>$display
        ], JSON_UNESCAPED_UNICODE);
        file_put_contents($cacheFile, $out);
        echo $out;
        exit;
    }

    // 最新一条：latest 用于在线状态，latest_gps 只给真实 GPS 位置。
    if (($_GET['latest'] ?? '') === '1') {
        $stmt = $db->query('SELECT recorded_at AS ts, temperature AS mv, humidity AS hum, rssi, fw, lat, lng, alt, spd, sat, fix, cell_csq AS csq, cell_sim AS sim, cell_net AS net, cell_tech AS tech, ota_status AS ota FROM sensor_data ORDER BY id DESC LIMIT 1');
        $row = $stmt->fetch();
        $gpsStmt = $db->query('SELECT recorded_at AS ts, temperature AS mv, humidity AS hum, rssi, fw, lat, lng, alt, spd, sat, fix, cell_csq AS csq, cell_sim AS sim, cell_net AS net, cell_tech AS tech, ota_status AS ota
                               FROM sensor_data
                               WHERE fix = 1 AND lat IS NOT NULL AND lng IS NOT NULL
                                 AND NOT (lat = 0 AND lng = 0)
                                 AND lat BETWEEN 18 AND 54 AND lng BETWEEN 73 AND 136
                               ORDER BY id DESC LIMIT 1');
        $gpsRow = $gpsStmt->fetch();
        echo json_encode(['status' => 'ok', 'latest' => $row ?: null, 'latest_gps' => $gpsRow ?: null], JSON_UNESCAPED_UNICODE);
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
    $gpsOnly = ($_GET['gps_only'] ?? '') === '1';
    $minDist = isset($_GET['min_dist']) ? max(0, (float)$_GET['min_dist']) : 0;
    $maxSpeed = isset($_GET['max_speed']) ? max(0, (float)$_GET['max_speed']) : 0;
    $minSat = (isset($_GET['min_sat']) && $_GET['min_sat'] !== '') ? max(0, (int)$_GET['min_sat']) : null;

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

    return [
        'from' => $fromDt,
        'to' => $toDt,
        'format' => $format,
        'gps_only' => $gpsOnly,
        'min_dist' => $minDist,
        'max_speed' => $maxSpeed,
        'min_sat' => $minSat,
    ];
}

// ==================== 查询 ====================
function queryData(PDO $db, array $params): array {
    $format = $params['format'];
    $where = ['recorded_at BETWEEN :from AND :to'];
    $exec = ['from' => $params['from'], 'to' => $params['to']];

    if ($params['gps_only']) {
        $where[] = 'fix = 1';
        $where[] = 'lat IS NOT NULL AND lng IS NOT NULL';
        $where[] = 'NOT (lat = 0 AND lng = 0)';
        $where[] = 'lat BETWEEN 18 AND 54';
        $where[] = 'lng BETWEEN 73 AND 136';
    }
    if ($params['min_sat'] !== null) {
        $where[] = 'sat >= :min_sat';
        $exec['min_sat'] = $params['min_sat'];
    }
    $whereSql = implode(' AND ', $where);

    if ($format === 'hourly') {
        $sql = "SELECT DATE_FORMAT(recorded_at, '%Y-%m-%d %H:00') AS ts,
                       AVG(temperature) AS temp, AVG(humidity) AS hum,
                       AVG(rssi) AS rssi, AVG(lat) AS lat, AVG(lng) AS lng,
                       AVG(alt) AS alt, AVG(spd) AS spd, AVG(sat) AS sat,
                       AVG(cell_csq) AS csq
                FROM sensor_data
                WHERE {$whereSql}
                GROUP BY ts ORDER BY ts ASC";
    } elseif ($format === 'daily') {
        $sql = "SELECT DATE(recorded_at) AS ts,
                       AVG(temperature) AS temp, AVG(humidity) AS hum,
                       AVG(rssi) AS rssi, AVG(lat) AS lat, AVG(lng) AS lng,
                       AVG(alt) AS alt, AVG(spd) AS spd, AVG(sat) AS sat,
                       AVG(cell_csq) AS csq
                FROM sensor_data
                WHERE {$whereSql}
                GROUP BY ts ORDER BY ts ASC";
    } else {
        // points: 返回所有原始点
        $sql = "SELECT recorded_at AS ts, temperature AS mv, humidity AS hum, rssi, fw, lat, lng, alt, spd, sat, fix, cell_csq AS csq, cell_sim AS sim, cell_net AS net, cell_tech AS tech, ota_status AS ota
                FROM sensor_data
                WHERE {$whereSql}
                ORDER BY recorded_at ASC";
    }

    $stmt = $db->prepare($sql);
    $stmt->execute($exec);
    $rows = $stmt->fetchAll();

    if ($format === 'points' && ($params['min_dist'] > 0 || $params['max_speed'] > 0)) {
        $rows = thinGpsPoints($rows, $params['min_dist'], $params['max_speed']);
    }
    return $rows;
}

function thinGpsPoints(array $rows, float $minDist, float $maxSpeed): array {
    $out = [];
    $last = null;
    foreach ($rows as $row) {
        if (!isValidGpsRow($row)) continue;
        if ($last === null) {
            $out[] = $row;
            $last = $row;
            continue;
        }
        $dist = distanceMeters((float)$last['lat'], (float)$last['lng'], (float)$row['lat'], (float)$row['lng']);
        if ($minDist > 0 && $dist < $minDist) continue;
        if ($maxSpeed > 0) {
            $dt = max(1, strtotime((string)$row['ts']) - strtotime((string)$last['ts']));
            $speed = ($dist / 1000) / ($dt / 3600);
            if ($speed > $maxSpeed) continue;
        }
        $out[] = $row;
        $last = $row;
    }
    return $out;
}

function isValidGpsRow(array $row): bool {
    $lat = isset($row['lat']) ? (float)$row['lat'] : 0;
    $lng = isset($row['lng']) ? (float)$row['lng'] : 0;
    return (int)($row['fix'] ?? 0) === 1
        && $lat !== 0.0 && $lng !== 0.0
        && $lat >= 18 && $lat <= 54
        && $lng >= 73 && $lng <= 136;
}

function distanceMeters(float $lat1, float $lng1, float $lat2, float $lng2): float {
    $dLat = ($lat2 - $lat1) * 111320;
    $dLng = ($lng2 - $lng1) * 111320 * cos(deg2rad(($lat1 + $lat2) / 2));
    return sqrt($dLat * $dLat + $dLng * $dLng);
}
