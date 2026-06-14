<?php
/**
 * ESP32 数据采集守护进程
 *
 * 使用方式：
 *   CLI 长期运行:   php subscribe_daemon.php
 *   Cron 每分钟:    php subscribe_daemon.php cron
 *   Web 触发:       GET /mmq/subscribe_daemon.php?mode=cron
 *
 * Cron 方式（推荐虚拟主机）:
 *   外部 cron 服务（如 cron-job.org）每 1-2 分钟请求一次这个 URL
 */

require_once __DIR__ . '/config.php';
require_once __DIR__ . '/mqtt_client.php';

set_time_limit(0);
date_default_timezone_set('Asia/Shanghai');

$isCLI = php_sapi_name() === 'cli';
$mode = $argv[1] ?? ($_GET['mode'] ?? 'daemon');

// 强制 cron 模式也按最大时长采集
define('FORCE_COLLECT_SECONDS', 58);

if ($mode === 'cron') {
    // Web 模式：缓冲杂散输出，最后清空缓冲输出纯 JSON
    if (!$isCLI) ob_start();
    $saved = collectBriefly();
    if (!$isCLI) ob_clean();
    outputResult($saved);
} else {
    runDaemon();
}

// ========== Cron 模式：短时间采集 ==========
function collectBriefly() {
    $db = getDB();
    $client = new MQTTClient(MQTT_BROKER, MQTT_PORT, MQTT_CLIENT_ID, MQTT_KEEPALIVE);

    $start = time();
    $deadline = $start + MQTT_COLLECT_SECONDS;
    $count = 0;

    if (!$client->connect(5)) {
        logLine("MQTT 连接失败");
        return $count;
    }

    if (!$client->subscribe(MQTT_TOPIC)) {
        logLine("订阅失败");
        $client->disconnect();
        return $count;
    }

    logLine("开始采集 " . MQTT_COLLECT_SECONDS . " 秒...");

    while (time() < $deadline) {
        $msg = $client->readMessage(10);
        if ($msg && $msg['topic'] === MQTT_TOPIC) {
            $data = json_decode($msg['payload'], true);
            if ($data) {
                saveData($db, $data);
                $count++;
            }
        }
    }

    $client->disconnect();
    logLine("采集完毕，共 {$count} 条");
    return $count;
}

// ========== 守护进程模式：一直运行 ==========
function runDaemon() {
    $db = getDB();

    while (true) {
        $client = new MQTTClient(MQTT_BROKER, MQTT_PORT, MQTT_CLIENT_ID, MQTT_KEEPALIVE);

        if ($client->connect(5) && $client->subscribe(MQTT_TOPIC)) {
            logLine("已连接，等待数据...");

            // 连续收 10 分钟，然后重连一次
            $reconnectAfter = time() + 600;
            while (time() < $reconnectAfter) {
                $msg = $client->readMessage(10);
                if ($msg && $msg['topic'] === MQTT_TOPIC) {
                    $data = json_decode($msg['payload'], true);
                    if ($data) saveData($db, $data);
                }
                if (!$msg) break; // 超时，重连
            }
        } else {
            logLine("连接失败，10 秒后重试");
        }

        $client->disconnect();
        sleep(10);
    }
}

// ========== 存储 ==========
function saveData(PDO $db, array $data) {
    try {
        $lat = isset($data['lat']) ? (float)$data['lat'] : 0.0;
        $lng = isset($data['lng']) ? (float)$data['lng'] : 0.0;
        $fix = isset($data['fix']) ? (int)$data['fix'] : 0;
        $hasGps = isRealGpsPoint($lat, $lng, $fix);
        if ($hasGps) {
            $stmt = $db->prepare(
                'INSERT INTO sensor_data (temperature, humidity, rssi, uptime, ip, lat, lng, alt, spd, sat, fix, cell_csq, recorded_at)
                 VALUES (:temp, :hum, :rssi, :uptime, :ip, :lat, :lng, :alt, :spd, :sat, :fix, :csq, NOW())'
            );
            $stmt->execute([
                'temp'   => $data['temp'] ?? null,
                'hum'    => $data['hum'] ?? null,
                'rssi'   => $data['bat'] ?? $data['rssi'] ?? null,
                'uptime' => $data['uptime'] ?? null,
                'ip'     => $data['ip'] ?? null,
                'lat'    => $lat,
                'lng'    => $lng,
                'alt'    => $data['alt'] ?? null,
                'spd'    => $data['spd'] ?? null,
                'sat'    => $data['sat'] ?? null,
                'fix'    => 1,
                'csq'    => $data['csq'] ?? null,
            ]);
        } else {
            if (!array_key_exists('temp', $data) && !array_key_exists('hum', $data)
                && !array_key_exists('rssi', $data) && !array_key_exists('uptime', $data)
                && !array_key_exists('ip', $data)) {
                return;
            }
            $stmt = $db->prepare(
                'INSERT INTO sensor_data (temperature, humidity, rssi, uptime, ip, recorded_at)
                 VALUES (:temp, :hum, :rssi, :uptime, :ip, NOW())'
            );
            $stmt->execute([
                'temp'   => $data['temp'] ?? null,
                'hum'    => $data['hum'] ?? null,
                'rssi'   => $data['bat'] ?? $data['rssi'] ?? null,
                'uptime' => $data['uptime'] ?? null,
                'ip'     => $data['ip'] ?? null,
            ]);
        }
    } catch (Exception $e) {
        logLine("DB 错误: " . $e->getMessage());
    }
}

function logLine($msg) {
    $ts = date('Y-m-d H:i:s');
    echo "[{$ts}] {$msg}\n";
}

function outputResult($count) {
    if (!headers_sent()) {
        header('Content-Type: application/json; charset=utf-8');
    }
    echo json_encode(['status' => 'ok', 'saved' => $count, 'time' => date('Y-m-d H:i:s')], JSON_UNESCAPED_UNICODE);
}

function isRealGpsPoint(float $lat, float $lng, int $fix): bool {
    return $fix === 1
        && !($lat === 0.0 && $lng === 0.0)
        && $lat >= 18 && $lat <= 54
        && $lng >= 73 && $lng <= 136;
}
