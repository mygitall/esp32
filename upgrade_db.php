<?php
/**
 * 数据库自动升级 — 浏览器访问一次即可
 * URL: https://www.sseeee.com/esp32/mmq/upgrade_db.php
 */
require_once __DIR__ . '/config.php';
header('Content-Type: text/plain; charset=utf-8');

try {
    $db = getDB();
    $driver = $db->getAttribute(PDO::ATTR_DRIVER_NAME);

    // 检查并添加 GPS + 蜂窝列
    $gpsCols = [
        'wifi_rssi'=>'INT','battery'=>'INT',
        'fw'=>'VARCHAR(32)',
        'lat'=>'DOUBLE','lng'=>'DOUBLE','alt'=>'DOUBLE','spd'=>'DOUBLE','sat'=>'INT','fix'=>'INT',
        'cell_csq'=>'INT','cell_sim'=>'VARCHAR(16)','cell_net'=>'VARCHAR(16)','cell_tech'=>'VARCHAR(16)',
        'ota_status'=>'VARCHAR(32)',
    ];

    foreach ($gpsCols as $col => $type) {
        $has = false;
        if ($driver === 'mysql') {
            $r = $db->query("SHOW COLUMNS FROM sensor_data LIKE '$col'");
            $has = (bool)$r->fetch();
        } else {
            $r = $db->query("PRAGMA table_info(sensor_data)");
            foreach ($r as $row) $has = $has || ($row['name'] === $col);
        }
        if (!$has) {
            $db->exec("ALTER TABLE sensor_data ADD COLUMN $col $type DEFAULT NULL");
            echo "✅ 添加列: $col\n";
        } else {
            echo "⏭ 已存在: $col\n";
        }
    }

    $indexes = [
        'idx_lat_lng' => 'CREATE INDEX idx_lat_lng ON sensor_data (lat, lng)',
        'idx_gps_recorded' => 'CREATE INDEX idx_gps_recorded ON sensor_data (`fix`, recorded_at)',
    ];
    foreach ($indexes as $name => $sql) {
        if (indexExists($db, $driver, $name)) {
            echo "⏭ 已存在索引: $name\n";
            continue;
        }
        $db->exec($sql);
        echo "✅ 添加索引: $name\n";
    }

    echo "\n🎉 数据库升级完成（GPS + 蜂窝列 + 查询索引）！可以关闭此页面。";
} catch (Exception $e) {
    echo "❌ 错误: " . $e->getMessage();
}

function indexExists(PDO $db, string $driver, string $name): bool {
    if ($driver === 'mysql') {
        $safe = str_replace("'", "''", $name);
        $r = $db->query("SHOW INDEX FROM sensor_data WHERE Key_name = '$safe'");
        return (bool)$r->fetch();
    }

    $r = $db->query('PRAGMA index_list(sensor_data)');
    foreach ($r as $row) {
        if (($row['name'] ?? '') === $name) return true;
    }
    return false;
}
