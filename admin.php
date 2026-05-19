<?php
/**
 * 数据管理 — 查看/清空记录
 * 访问：https://www.sseeee.com/esp32/mmq/admin.php
 */

require_once __DIR__ . '/config.php';

$action = $_GET['a'] ?? '';

// ========== 清空数据 ==========
if ($action === 'clear') {
    try {
        $db = getDB();
        $db->exec('TRUNCATE TABLE sensor_data');
        $msg = '已清空全部数据';
    } catch (Exception $e) {
        $msg = '清空失败: ' . $e->getMessage();
    }
}

// ========== 查询统计 ==========
try {
    $db = getDB();
    $total = $db->query('SELECT COUNT(*) FROM sensor_data')->fetchColumn();
    $last = $db->query('SELECT MAX(recorded_at) FROM sensor_data')->fetchColumn();
    $first = $db->query('SELECT MIN(recorded_at) FROM sensor_data')->fetchColumn();
} catch (Exception $e) {
    $total = 0; $last = '--'; $first = '--';
}
?><!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32 数据管理</title>
<style>
  * { margin: 0; padding: 0; box-sizing: border-box; }
  body {
    font-family: -apple-system, 'Segoe UI', sans-serif;
    background: #0b0e14; color: #e2e4e9;
    display: flex; justify-content: center; align-items: center;
    min-height: 100vh; padding: 20px;
  }
  .card {
    background: #141820; border: 1px solid #252a35;
    border-radius: 16px; padding: 24px;
    width: 100%; max-width: 400px; text-align: center;
  }
  h1 { font-size: 1.2rem; margin-bottom: 20px; color: #4f8fff; }
  .stats {
    display: grid; grid-template-columns: 1fr 1fr;
    gap: 10px; margin-bottom: 20px;
  }
  .stat {
    background: rgba(255,255,255,0.03); border-radius: 10px; padding: 12px;
  }
  .stat .num { font-size: 1.8rem; font-weight: 700; color: #4f8fff; }
  .stat .lbl { font-size: 0.7rem; color: #5a5e6a; margin-top: 2px; }
  .info { font-size: 0.75rem; color: #5a5e6a; margin-bottom: 16px; }
  .btn {
    display: block; width: 100%; padding: 14px; border: none;
    border-radius: 12px; font-size: 1rem; font-weight: 600;
    cursor: pointer; transition: all .2s; margin-top: 8px;
  }
  .btn-danger { background: #ff4757; color: #fff; }
  .btn-danger:hover { opacity: 0.85; }
  .btn-back { background: #252a35; color: #8b8f9a; }
  .msg { padding: 10px; border-radius: 8px; margin-bottom: 12px; font-size: 0.85rem; }
  .msg.ok { background: rgba(0,212,170,0.12); color: #00d4aa; }
  .msg.err { background: rgba(255,71,87,0.12); color: #ff4757; }
</style>
</head>
<body>
<div class="card">
  <h1>ESP32 数据管理</h1>

  <?php if (isset($msg)): ?>
    <div class="msg ok"><?= htmlspecialchars($msg) ?></div>
  <?php endif; ?>

  <div class="stats">
    <div class="stat">
      <div class="num"><?= number_format($total) ?></div>
      <div class="lbl">总记录数</div>
    </div>
    <div class="stat">
      <div class="num" style="font-size:0.9rem;word-break:break-all"><?= $first !== '--' ? substr($first, 0, 16) : '--' ?></div>
      <div class="lbl">最早记录</div>
    </div>
  </div>

  <div class="info">最近记录：<?= $last !== '--' ? substr($last, 0, 16) : '--' ?></div>

  <button class="btn btn-danger" onclick="clearAll()">🗑 清空全部数据</button>
  <button class="btn btn-back" onclick="location.reload()">刷新</button>
</div>

<script>
function clearAll() {
  if (!confirm('确定要删除全部记录吗？此操作不可恢复。')) return;
  if (!confirm('再次确认：删除全部 ' + <?= $total ?> + ' 条记录？')) return;
  location.href = '?a=clear';
}
</script>
</body>
</html>
