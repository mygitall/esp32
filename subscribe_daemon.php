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

$mode = $argv[1] ?? ($_GET['mode'] ?? 'daemon');

if ($mode === 'cron') {
    collectBriefly();
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
        outputResult(0);
        return;
    }

    if (!$client->subscribe(MQTT_TOPIC)) {
        logLine("订阅失败");
        $client->disconnect();
        outputResult(0);
        return;
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
    outputResult($count);
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
        $stmt = $db->prepare(
            'INSERT INTO sensor_data (temperature, humidity, rssi, uptime, ip, recorded_at)
             VALUES (:temp, :hum, :rssi, :uptime, :ip, NOW())'
        );
        $stmt->execute([
            'temp'  => $data['temp'] ?? null,
            'hum'   => $data['hum'] ?? null,
            'rssi'  => $data['rssi'] ?? null,
            'uptime'=> $data['uptime'] ?? null,
            'ip'    => $data['ip'] ?? null,
        ]);
    } catch (Exception $e) {
        logLine("DB 错误: " . $e->getMessage());
    }
}

function logLine($msg) {
    $ts = date('Y-m-d H:i:s');
    echo "[{$ts}] {$msg}\n";
}

function outputResult($count) {
    header('Content-Type: application/json');
    echo json_encode(['status' => 'ok', 'saved' => $count, 'time' => date('Y-m-d H:i:s')]);
}
