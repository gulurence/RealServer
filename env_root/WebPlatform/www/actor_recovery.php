<?php
// =============================================================================
// Actor 动态恢复记录 — 读 actor:recovery:log
// 来源: Actor 帧级脏读写框架 (P7) 在 save/recover 时写入的事件。
// =============================================================================
require_once __DIR__ . '/_layout.php';

$r = redis();

const RECOVERY_LOG_KEY = 'actor:recovery:log';

function parse_recovery_line(string $line): array
{
    $p = explode('|', $line, 5);
    return [
        'ts'    => (int)($p[0] ?? 0),
        'type'  => $p[1] ?? '',
        'id'    => $p[2] ?? '',
        'event' => $p[3] ?? '',
        'detail'=> $p[4] ?? '',
    ];
}

function rec_event_badge(string $e): string
{
    $map = [
        'saved'     => ['状态落库', 'text-bg-success'],
        'recovered' => ['动态恢复', 'text-bg-success'],
        'failed'    => ['恢复失败', 'text-bg-danger'],
        'restored'  => ['已重建', 'text-bg-success'],
    ];
    $m = $map[$e] ?? [$e, 'offline'];
    return '<span class="badge ' . $m[1] . '">' . esc($m[0]) . '</span>';
}

$rows = [];
$actorStates = 0;
if ($r !== null) {
    $raw = $r->lrange(RECOVERY_LOG_KEY, 0, 199);
    if (is_array($raw)) {
        foreach (array_reverse($raw) as $line) {
            $rows[] = parse_recovery_line((string)$line);
        }
        usort($rows, fn($a, $b) => $b['ts'] <=> $a['ts']);
    }
    // 当前 Redis 中的 Actor 状态快照数量
    try {
        $keys = scan_cluster($r, 'actor:state:*');
        $actorStates = count($keys);
    } catch (Throwable $e) {}
}

layout_header('Actor 恢复', 'actor_recovery');
autorefresh();
live_region_start();
?>
<div class="stat-grid">
    <div class="stat"><div class="k">当前状态快照数</div><div class="v"><?= $actorStates ?></div></div>
    <div class="stat"><div class="k">恢复事件数 (最近)</div><div class="v"><?= count($rows) ?></div></div>
</div>

<div class="card">
    <div class="card-header">Actor 动态恢复记录 <span class="dim">actor:recovery:log</span></div>
    <div class="card-body">
        <?php if ($r === null): ?>
            <div class="empty">⚠️ 无法连接 Redis Cluster。</div>
        <?php elseif (empty($rows)): ?>
            <div class="empty">暂无恢复记录 —— Actor 帧级脏读写框架会在此记录 save / recover 事件。</div>
        <?php else: ?>
            <div class="table-responsive"><table class="table table-vcenter">
                <thead><tr>
                    <th>时间</th><th>Actor 类型</th><th>Actor ID</th><th>事件</th><th>详情</th>
                </tr></thead>
                <tbody>
                <?php foreach ($rows as $row): ?>
                    <tr>
                        <td class="mono"><?= esc($row['ts'] > 0 ? date('m-d H:i:s', $row['ts']) : '-') ?></td>
                        <td class="mono"><?= esc($row['type']) ?></td>
                        <td class="mono"><?= esc($row['id']) ?></td>
                        <td><?= rec_event_badge($row['event']) ?></td>
                        <td class="mono" style="word-break:break-all"><?= esc($row['detail']) ?></td>
                    </tr>
                <?php endforeach; ?>
                </tbody>
            </table></div>
        <?php endif; ?>
    </div>
</div>

<?php live_region_end(); ?>
<?php layout_footer(); ?>
