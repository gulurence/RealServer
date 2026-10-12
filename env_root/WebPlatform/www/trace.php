<?php
// =============================================================================
// 链路追踪 — 延迟阈值配置
//
// 写入 Redis trace:config (hash) 字段 latency_threshold_ms (毫秒)。
// RealServer 的 CTraceManager 每 10 秒刷新读取该阈值, 仅当协议延迟超过阈值
// (慢消息) 时才上报到 trace:link:*:*:*:* ZSET, 供 trace_topology.php 展示。
// =============================================================================
require_once __DIR__ . '/_layout.php';

const TRACE_CONFIG_KEY   = 'trace:config';
const TRACE_CONFIG_FIELD = 'latency_threshold_ms';
const DEFAULT_THRESHOLD  = 50;

$r = redis();
$saved = false;
$err   = '';

// 处理表单提交
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $raw = trim((string)($_POST['threshold'] ?? ''));
    if ($raw !== '' && ctype_digit($raw)) {
        $value = (int)$raw;
        if ($value <= 0) {
            $err = '阈值必须为正整数 (毫秒)';
        } elseif ($r !== null) {
            $r->hSet(TRACE_CONFIG_KEY, TRACE_CONFIG_FIELD, (string)$value);
            $saved = true;
        } else {
            $err = '无法连接 Redis, 写入失败';
        }
    } else {
        $err = '请输入合法的正整数 (毫秒)';
    }
}

// 读取当前阈值
$threshold = DEFAULT_THRESHOLD;
if ($r !== null) {
    $v = $r->hGet(TRACE_CONFIG_KEY, TRACE_CONFIG_FIELD);
    if ($v !== false && ctype_digit((string)$v)) {
        $threshold = (int)$v;
    }
}
?>
<?php layout_header('链路追踪', 'trace'); ?>
        <div class="summary">
            链路追踪延迟阈值：<b><?= $threshold ?> ms</b>
            <span class="dim">（RealServer 每 10 秒刷新；超过阈值的慢消息才会被上报）</span>
        </div>

        <nav class="trace-nav" style="margin-bottom:20px;">
            <a href="trace.php" class="active">阈值配置</a>
            <a href="trace_topology.php">拓扑图</a>
        </nav>

        <?php if ($r === null): ?>
            <div class="empty">⚠️ 无法连接 Redis，请确认 Redis 已启动。</div>
        <?php else: ?>
            <?php if ($saved): ?>
                <div class="empty" style="border-color:#22c55e;color:#22c55e;">✅ 阈值已更新为 <?= $threshold ?> ms</div>
            <?php endif; ?>
            <?php if ($err !== ''): ?>
                <div class="empty" style="border-color:#f87171;color:#f87171;">⚠️ <?= esc($err) ?></div>
            <?php endif; ?>

            <section class="node-group">
                <h2>设置延迟阈值</h2>
                <form method="post" action="trace.php" style="display:flex;gap:12px;align-items:center;flex-wrap:wrap;">
                    <label style="color:var(--dim);font-size:14px;">
                        阈值 (毫秒)
                        <input class="form-control" type="number" name="threshold" min="1" step="1"
                               value="<?= $threshold ?>"
                               style="margin-left:8px;padding:8px 12px;border-radius:8px;border:1px solid var(--border);background:var(--bg);color:var(--text);font-size:14px;width:120px;">
                    </label>
                    <button type="submit"
                            style="padding:9px 18px;border:0;border-radius:8px;background:var(--accent);color:#fff;font-size:14px;cursor:pointer;">
                        保存
                    </button>
                </form>
                <p class="dim" style="margin-top:12px;">
                    写入键：<code>trace:config</code> 字段 <code>latency_threshold_ms</code>。
                    调低阈值会捕获更多慢消息；调高则只捕获显著延迟的消息。
                </p>
            </section>
        <?php endif; ?>
<?php layout_footer(); ?>
