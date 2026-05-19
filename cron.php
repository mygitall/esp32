<?php
/**
 * Cron 触发器 — 供外部 cron 服务调用
 * 在 cron-job.org 或类似服务中设置:
 *   URL: https://你的域名/mmq/cron.php
 *   间隔: 每 1-2 分钟
 */

require_once __DIR__ . '/subscribe_daemon.php';
