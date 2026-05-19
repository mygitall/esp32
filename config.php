<?php
// ==================== 数据库配置 ====================
// 上传虚拟主机时修改这里
define('DB_HOST', '127.0.0.1');
define('DB_PORT', '8889');
define('DB_NAME', 'esp32_monitor');
define('DB_USER', 'root');
define('DB_PASS', 'root');
define('DB_CHARSET', 'utf8mb4');

// ==================== MQTT 配置 ====================
define('MQTT_BROKER', 'broker.emqx.io');
define('MQTT_PORT', 1883);
define('MQTT_TOPIC', 'esp32/status');       // 订阅主题
define('MQTT_CLIENT_ID', 'php_sub_' . uniqid());
define('MQTT_KEEPALIVE', 30);               // 秒
define('MQTT_COLLECT_SECONDS', 55);         // 每次运行采集多少秒（cron 每分钟触发）

// ==================== 数据库连接 ====================
function getDB(): PDO {
    static $pdo = null;
    if ($pdo === null) {
        $dsn = 'mysql:host=' . DB_HOST . ';port=' . DB_PORT . ';dbname=' . DB_NAME . ';charset=' . DB_CHARSET;
        $pdo = new PDO($dsn, DB_USER, DB_PASS, [
            PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
            PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
            PDO::ATTR_EMULATE_PREPARES => false,
        ]);
    }
    return $pdo;
}
