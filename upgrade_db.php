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

    // 检查并添加 GPS 列
    $gpsCols = ['lat'=>'DOUBLE','lng'=>'DOUBLE','alt'=>'DOUBLE','spd'=>'DOUBLE','sat'=>'INT','fix'=>'INT'];

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
    echo "\n🎉 数据库升级完成！可以关闭此页面。";
} catch (Exception $e) {
    echo "❌ 错误: " . $e->getMessage();
}
