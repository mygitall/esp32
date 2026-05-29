<?php
/**
 * OSRM 路网匹配代理 — GPS 坐标吸附到真实道路
 *
 * POST /match_proxy.php  Body: JSON { points: [{lat,lng,ts?},...] }
 * 返回: { status:"ok", matched:[{lat,lng},...] }
 *
 * 上传到: www.sseeee.com/esp32/mmq/match_proxy.php
 */
header('Content-Type: application/json; charset=utf-8');
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: POST, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type');
if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') { http_response_code(204); exit; }

$body = json_decode(file_get_contents('php://input'), true);
if (!$body || empty($body['points'])) {
    http_response_code(400);
    echo json_encode(array('status' => 'error', 'message' => 'need {points:[...]}'));
    exit;
}

// === 1. 去重：过滤位移 < 5m 的连续重复点 ===
$raw = $body['points'];
$pts = array();
foreach ($raw as $p) {
    $lat = isset($p['lat']) ? (float)$p['lat'] : 0;
    $lng = isset($p['lng']) ? (float)$p['lng'] : 0;
    if (!$lat || !$lng) continue;

    if (count($pts) === 0) {
        $pts[] = array('lat' => $lat, 'lng' => $lng, 'ts' => isset($p['ts']) ? $p['ts'] : null);
        continue;
    }

    $prev = $pts[count($pts) - 1];
    $dLat = ($lat - $prev['lat']) * 111320;
    $dLng = ($lng - $prev['lng']) * 111320 * cos(deg2rad($prev['lat']));
    if (sqrt($dLat * $dLat + $dLng * $dLng) >= 5) {
        $pts[] = array('lat' => $lat, 'lng' => $lng, 'ts' => isset($p['ts']) ? $p['ts'] : null);
    }
}

if (count($pts) < 3) {
    echo json_encode(array('status' => 'ok', 'matched' => $pts, 'note' => 'too few points'));
    exit;
}

// === 2. 分批调用 OSRM ===
$BATCH = 80;
$matched = array();
$SERVERS = array(
    'https://router.project-osrm.org/match/v1/driving/',
    'https://routing.openstreetmap.de/routed-car/match/v1/driving/',
);

for ($i = 0; $i < count($pts); $i += $BATCH) {
    $batch = array_slice($pts, $i, $BATCH);
    if (count($batch) < 3) {
        foreach ($batch as $p) $matched[] = $p;
        continue;
    }

    // 拼坐标
    $coordParts = array();
    foreach ($batch as $p) $coordParts[] = $p['lng'] . ',' . $p['lat'];
    $coords = implode(';', $coordParts);

    // 半径
    $radii = implode(';', array_fill(0, count($batch), '30'));

    // 时间戳
    $tsParts = array();
    foreach ($batch as $p) {
        $t = 0;
        if (!empty($p['ts'])) {
            $parsed = strtotime((string)$p['ts']);
            if ($parsed) $t = $parsed;
        }
        $tsParts[] = (string)$t;
    }
    $tss = implode(';', $tsParts);

    $query = $coords . '?geometries=geojson&overview=full&radiuses=' . $radii . '&timestamps=' . $tss . '&gaps=ignore';

    $ok = false;
    foreach ($SERVERS as $base) {
        $url = $base . $query;
        $ctx = stream_context_create(array('http' => array(
            'timeout' => 15,
            'header' => "User-Agent: ESP32-GPS/1.0\r\n"
        )));
        $resp = @file_get_contents($url, false, $ctx);
        if ($resp === false) continue;

        $data = json_decode($resp, true);
        $code = isset($data['code']) ? $data['code'] : '';
        if ($code === 'Ok' && !empty($data['matchings'][0]['geometry']['coordinates'])) {
            foreach ($data['matchings'][0]['geometry']['coordinates'] as $c) {
                $matched[] = array('lat' => (float)$c[1], 'lng' => (float)$c[0]);
            }
            $ok = true;
            break;
        }
    }

    // OSRM 失败 → 回退原始数据
    if (!$ok) {
        foreach ($batch as $p) $matched[] = $p;
    }
}

echo json_encode(array(
    'status' => 'ok',
    'matched' => $matched,
    'count' => count($matched)
), JSON_UNESCAPED_UNICODE);
