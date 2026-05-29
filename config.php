<?php
// ==================== 数据库配置 ====================
// 上传虚拟主机时修改这里
define('DB_HOST', '127.0.0.1');
define('DB_PORT', '8889');
define('DB_NAME', 'esp32_monitor');
define('DB_USER', 'root');
define('DB_PASS', 'root');
// 老版本 MySQL 可能不支持 utf8mb4；连接失败时会自动回退到 utf8。
define('DB_CHARSET', 'utf8mb4');
define('DB_FALLBACK_CHARSET', 'utf8');

date_default_timezone_set('Asia/Shanghai');  // 北京时间

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
        $options = [
            PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
            PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
            PDO::ATTR_EMULATE_PREPARES => false,
        ];
        foreach ([DB_CHARSET, DB_FALLBACK_CHARSET] as $charset) {
            try {
                $dsn = 'mysql:host=' . DB_HOST . ';port=' . DB_PORT . ';dbname=' . DB_NAME . ';charset=' . $charset;
                $pdo = new PDO($dsn, DB_USER, DB_PASS, $options);
                break;
            } catch (PDOException $e) {
                if (stripos($e->getMessage(), 'Unknown character set') === false || $charset === DB_FALLBACK_CHARSET) {
                    throw $e;
                }
            }
        }
    }
    return $pdo;
}
