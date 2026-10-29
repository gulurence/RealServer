<?php
// =============================================================================
// Actor 动态恢复记录 — 读 actor:recovery:log
// 来源: Actor 帧级脏读写框架 (P7) 在 save/recover 时写入的事件。
// =============================================================================
require_once __DIR__ . '/_layout.php';

$r = redis();

const RECOVERY_LOG_KEY = 'actor:recovery:log';
const RECOVERY_PER_PAGE = 8;   // Actor 动态恢复记录每页条数

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

/** 恢复记录分页/查询链接: 保留 q/page, 覆盖指定参数 (null/'' 移除) */
function recovery_href(array $overrides): string
{
    $params = [];
    foreach (['q', 'page'] as $k) {
        if (isset($_GET[$k]) && (string)$_GET[$k] !== '') {
            $params[$k] = (string)$_GET[$k];
        }
    }
    foreach ($overrides as $k => $v) {
        if ($v === null || $v === '') {
            unset($params[$k]);
        } else {
            $params[$k] = (string)$v;
        }
    }
    $qs = http_build_query($params);
    return 'actor_recovery.php' . ($qs !== '' ? '?' . $qs : '');
}

function recovery_page_href(int $page): string
{
    return recovery_href(['page' => $page]);
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
$total = count($rows);

// Actor ID 查询 (精确匹配)
$q = trim((string)($_GET['q'] ?? ''));
if ($q !== '') {
    $rows = array_values(array_filter($rows, fn($r) => (string)$r['id'] === $q));
}
$filteredTotal = count($rows);

// 分页: 每页 RECOVERY_PER_PAGE 条
$pages = max(1, (int)ceil($filteredTotal / RECOVERY_PER_PAGE));
$page  = max(1, min((int)($_GET['page'] ?? 1), $pages));
$pageRows = array_slice($rows, ($page - 1) * RECOVERY_PER_PAGE, RECOVERY_PER_PAGE);

layout_header('Actor 恢复', 'actor_recovery');
autorefresh();
if (!is_partial() && $r !== null): ?>
<div class="d-flex flex-wrap gap-3" style="margin-bottom:14px;">
    <form method="get" action="actor_recovery.php" class="form-row" style="flex:1 1 340px; margin:0;">
        <label style="flex:1 1 240px;">
            <span>Actor ID 查询</span>
            <input type="text" name="q" value="<?= esc($q) ?>" class="form-control" placeholder="输入 Actor ID">
        </label>
        <button type="submit" class="btn btn-primary">查询</button>
        <?php if ($q !== ''): ?><a class="btn btn-link" href="<?= esc(recovery_href(['q' => null, 'page' => null])) ?>">清除</a><?php endif; ?>
    </form>
</div>
<?php endif;
live_region_start();
?>
<div class="stat-grid">
    <div class="stat"><div class="k">当前状态快照数</div><div class="v"><?= $actorStates ?></div></div>
    <div class="stat"><div class="k">恢复事件数 (最近)</div><div class="v"><?= $total ?></div></div>
    <?php if ($q !== ''): ?><div class="stat"><div class="k">匹配 Actor ID</div><div class="v"><?= $filteredTotal ?></div></div><?php endif; ?>
</div>

<div class="card">
    <div class="card-header">Actor 动态恢复记录 <span class="dim">actor:recovery:log</span></div>
    <div class="card-body">
        <?php if ($r === null): ?>
            <div class="empty">⚠️ 无法连接 Redis Cluster。</div>
        <?php elseif ($total === 0): ?>
            <div class="empty">暂无恢复记录 —— Actor 帧级脏读写框架会在此记录 save / recover 事件。</div>
        <?php elseif ($filteredTotal === 0): ?>
            <div class="empty">没有匹配 Actor ID「<?= esc($q) ?>」的恢复记录。</div>
        <?php else: ?>
            <div class="table-responsive"><table class="table table-vcenter">
                <thead><tr>
                    <th>时间</th><th>Actor 类型</th><th>Actor ID</th><th>事件</th><th>详情</th>
                </tr></thead>
                <tbody>
                <?php foreach ($pageRows as $row): ?>
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

            <?php if ($pages > 1): ?>
            <div class="actions" style="justify-content:space-between; align-items:center; flex-wrap:wrap;">
                <div class="dim">第 <?= $page ?> / <?= $pages ?> 页 · 共 <?= $filteredTotal ?> 条</div>
                <div class="actions">
                    <?php if ($page > 1): ?>
                        <a class="btn btn-sm btn-outline-secondary" href="<?= esc(recovery_page_href($page - 1)) ?>">上一页</a>
                    <?php endif; ?>
                    <?php if ($page < $pages): ?>
                        <a class="btn btn-sm btn-outline-secondary" href="<?= esc(recovery_page_href($page + 1)) ?>">下一页</a>
                    <?php endif; ?>
                </div>
            </div>
            <?php endif; ?>
        <?php endif; ?>
    </div>
</div>

<?php live_region_end(); ?>
<?php layout_footer(); ?>
