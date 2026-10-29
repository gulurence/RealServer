<?php
// =============================================================================
// 进程宕机记录 — 读 cluster:crash:log (NodeManager 专用崩溃/拉起事件)
// 兜底也扫 cluster:alert 里含 crash/fault 的告警。
// 仅显示最新 20 条, 支持按事件 (?event=) 过滤。
// =============================================================================
require_once __DIR__ . '/_layout.php';

$r = redis();

const CRASH_LOG_KEY = 'cluster:crash:log';

/**
 * 解析崩溃日志条目: "{ts}|{machine}|{type}|{name}|{event}|{detail}"
 */
function parse_crash_line(string $line): array
{
    $p = explode('|', $line, 6);
    return [
        'ts'      => (int)($p[0] ?? 0),
        'machine' => $p[1] ?? '',
        'type'    => $p[2] ?? '',
        'name'    => $p[3] ?? '',
        'event'   => $p[4] ?? '',
        'detail'  => $p[5] ?? '',
    ];
}

function fmt_ts(int $ts): string
{
    return $ts > 0 ? date('m-d H:i:s', $ts) : '-';
}

// 事件类型 → [显示名, 徽章样式]。过滤下拉也复用此表。
const CRASH_EVENTS = [
    'crash'     => ['崩溃', 'text-bg-danger'],
    'faulted'   => ['熔断放弃', 'text-bg-warning'],
    'restarted' => ['已拉起', 'text-bg-success'],
    'stopped'   => ['已停止', 'text-bg-secondary'],
];

function event_badge(string $e): string
{
    $m = CRASH_EVENTS[$e] ?? [$e, 'offline'];
    return '<span class="badge ' . $m[1] . '">' . esc($m[0]) . '</span>';
}

// 事件过滤 (?event=, 非法值回退为「全部」)
$eventFilter = trim((string)($_GET['event'] ?? ''));
if ($eventFilter !== '' && !isset(CRASH_EVENTS[$eventFilter])) $eventFilter = '';

$rows = [];
if ($r !== null) {
    // 专用崩溃日志 (最新在前)
    $raw = $r->lrange(CRASH_LOG_KEY, 0, 199);
    if (is_array($raw)) {
        foreach (array_reverse($raw) as $line) {
            $rows[] = parse_crash_line((string)$line);
        }
    }
    // 兜底: 扫 cluster:alert 的崩溃/熔断告警
    $rawAlert = $r->lrange(ALERT_KEY, 0, 199);
    if (is_array($rawAlert)) {
        foreach (array_reverse($rawAlert) as $line) {
            $s = (string)$line;
            if (stripos($s, 'crash') === false && stripos($s, 'fault') === false && stripos($s, 'exited') === false) {
                continue;
            }
            $rows[] = ['ts' => (int)strtok($s, ' '), 'machine' => '', 'type' => '', 'name' => '',
                       'event' => 'crash', 'detail' => $s];
        }
    }
    // 按时间倒序
    usort($rows, fn($a, $b) => $b['ts'] <=> $a['ts']);
}

// 按事件过滤
if ($eventFilter !== '') {
    $rows = array_values(array_filter($rows, fn($row) => ($row['event'] ?? '') === $eventFilter));
}
// 只保留最新 20 条
$rows = array_slice($rows, 0, 20);

layout_header('进程宕机', 'crashes');
autorefresh();
live_region_start();
?>
<div class="card">
    <div class="card-header">进程宕机 / 拉起记录 <span class="dim">cluster:crash:log · 最新 20 条</span></div>
    <div class="card-body">
        <?php if ($r === null): ?>
            <div class="empty">⚠️ 无法连接 Redis Cluster。</div>
        <?php else: ?>
            <!-- 事件过滤 -->
            <form method="get" action="crashes.php" class="form-row" style="margin-bottom:14px;">
                <label>
                    <span>按事件过滤</span>
                    <select name="event" class="form-select">
                        <option value="">全部事件</option>
                        <?php foreach (CRASH_EVENTS as $ev => $info): ?>
                            <option value="<?= esc($ev) ?>" <?= $eventFilter === $ev ? 'selected' : '' ?>><?= esc($info[0]) ?></option>
                        <?php endforeach; ?>
                    </select>
                </label>
                <button type="submit" class="btn btn-primary">过滤</button>
                <?php if ($eventFilter !== ''): ?><a class="btn btn-link" href="crashes.php">清除</a><?php endif; ?>
            </form>

            <?php if (empty($rows)): ?>
                <div class="empty"><?= $eventFilter !== '' ? '该事件下暂无记录。' : '暂无宕机记录 —— 进程崩溃后 NodeManager 会写入拉起/熔断事件。' ?></div>
            <?php else: ?>
                <div class="table-responsive"><table class="table table-vcenter">
                    <thead><tr>
                        <th>时间</th><th>机器</th><th>类型</th><th>进程名</th><th>事件</th><th>详情</th>
                    </tr></thead>
                    <tbody>
                    <?php foreach ($rows as $row): ?>
                        <tr>
                            <td class="mono"><?= esc(fmt_ts($row['ts'])) ?></td>
                            <td class="mono"><?= esc($row['machine']) ?></td>
                            <td><?= esc($row['type']) ?></td>
                            <td><?= esc($row['name']) ?></td>
                            <td><?= event_badge($row['event']) ?></td>
                            <td class="mono" style="word-break:break-all"><?= esc($row['detail']) ?></td>
                        </tr>
                    <?php endforeach; ?>
                    </tbody>
                </table></div>
            <?php endif; ?>
        <?php endif; ?>
    </div>
</div>

<?php live_region_end(); ?>
<?php layout_footer(); ?>
