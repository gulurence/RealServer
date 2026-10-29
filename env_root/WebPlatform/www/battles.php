<?php
// 战斗战报列表 — 读 Redis battle:index 有序集合 (ZADD, score=结算时间戳) 倒序展示
require_once __DIR__ . '/_layout.php';

/**
 * 解析战报摘要 "win|finalHash|frameCount|timestamp|playerCount|roster"
 * @return array{win:string,hash:string,frames:string,ts:string,players:string,roster:string}
 */
function parse_summary(?string $s): array
{
    $p = explode('|', (string)$s);
    return [
        'win'     => $p[0] ?? '-',
        'hash'    => $p[1] ?? '-',
        'frames'  => $p[2] ?? '-',
        'ts'      => $p[3] ?? '0',
        'players' => $p[4] ?? '-',
        'roster'  => $p[5] ?? '-',
    ];
}

function fmt_time(?string $ts): string
{
    $i = (int)$ts;
    return $i > 0 ? date('Y-m-d H:i:s', $i) : '-';
}

const BATTLES_PER_PAGE = 10;   // 战斗战报每页条数

/** 战斗战报分页/查询链接: 保留 q/page, 覆盖指定参数 (null/'' 移除) */
function battles_href(array $overrides): string
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
    return 'battles.php' . ($qs !== '' ? '?' . $qs : '');
}

function battles_page_href(int $page): string
{
    return battles_href(['page' => $page]);
}

$r = redis();
$battles = [];
if ($r !== null) {
    // member => score (battleId => timestamp), 倒序
    $index = $r->zRevRange('battle:index', 0, -1, true);
    if (is_array($index)) {
        foreach ($index as $battleId => $score) {
            $summary = parse_summary($r->get("battle:result:{$battleId}"));
            $battles[] = ['id' => $battleId, 'ts' => $score, 'summary' => $summary];
        }
    }
}
$totalAll = count($battles);

// 查询过滤: 只按 战斗ID 精确查询
$q = trim((string)($_GET['q'] ?? ''));
if ($q !== '') {
    $battles = array_values(array_filter($battles, function ($b) use ($q) {
        return (string)$b['id'] === $q;
    }));
}
$filteredTotal = count($battles);

// 分页: 每页 BATTLES_PER_PAGE 条
$pages = max(1, (int)ceil($filteredTotal / BATTLES_PER_PAGE));
$page  = max(1, min((int)($_GET['page'] ?? 1), $pages));
$pageBattles = array_slice($battles, ($page - 1) * BATTLES_PER_PAGE, BATTLES_PER_PAGE);
?>
<?php layout_header('战斗战报', 'battles'); ?>
<?php autorefresh(); ?>
<?php if (!is_partial() && $r !== null): ?>
<div class="d-flex flex-wrap gap-3" style="margin-bottom:14px;">
    <form method="get" action="battles.php" class="form-row" style="flex:1 1 340px; margin:0;">
        <label style="flex:1 1 260px;">
            <span>查询（战斗ID）</span>
            <input type="text" name="q" value="<?= esc($q) ?>" class="form-control" placeholder="输入战斗ID">
        </label>
        <button type="submit" class="btn btn-primary">查询</button>
        <?php if ($q !== ''): ?><a class="btn btn-link" href="<?= esc(battles_href(['q' => null, 'page' => null])) ?>">清除</a><?php endif; ?>
    </form>
</div>
<?php endif; ?>
<?php live_region_start(); ?>
        <div class="summary">
            战斗总数：<b><?= $totalAll ?></b>
            <?php if ($q !== ''): ?> · 匹配战斗：<b><?= $filteredTotal ?></b><?php endif; ?>
            <span class="dim">（按结算时间倒序，每 5 秒自动刷新）</span>
        </div>

        <?php if ($r === null): ?>
            <div class="empty">⚠️ 无法连接 Redis，请确认 Redis 已启动且战斗已结算。</div>
        <?php elseif ($totalAll === 0): ?>
            <div class="empty">暂无战斗战报 —— 战斗结算后会自动写入 <code>battle:index</code>。</div>
        <?php elseif ($filteredTotal === 0): ?>
            <div class="empty">没有匹配「<?= esc($q) ?>」的战斗。</div>
        <?php else: ?>
            <table class="table table-vcenter">
                <thead>
                    <tr>
                        <th>战斗ID</th><th>胜负</th><th>最终哈希</th><th>帧数</th>
                        <th>人数</th><th>结算时间</th><th></th>
                    </tr>
                </thead>
                <tbody>
                <?php foreach ($pageBattles as $b): ?>
                    <tr>
                        <td class="mono"><?= esc($b['id']) ?></td>
                        <td><span class="badge"><?= esc($b['summary']['win']) ?></span></td>
                        <td class="mono"><?= esc($b['summary']['hash']) ?></td>
                        <td><?= esc($b['summary']['frames']) ?></td>
                        <td><?= esc($b['summary']['players']) ?></td>
                        <td><?= esc(fmt_time($b['ts'])) ?></td>
                        <td><a href="battle.php?id=<?= esc($b['id']) ?>">详情</a></td>
                    </tr>
                <?php endforeach; ?>
                </tbody>
            </table>

            <?php if ($pages > 1): ?>
            <div class="actions" style="justify-content:space-between; align-items:center; flex-wrap:wrap;">
                <div class="dim">第 <?= $page ?> / <?= $pages ?> 页 · 共 <?= $filteredTotal ?> 场战斗</div>
                <div class="actions">
                    <?php if ($page > 1): ?>
                        <a class="btn btn-sm btn-outline-secondary" href="<?= esc(battles_page_href($page - 1)) ?>">上一页</a>
                    <?php endif; ?>
                    <?php if ($page < $pages): ?>
                        <a class="btn btn-sm btn-outline-secondary" href="<?= esc(battles_page_href($page + 1)) ?>">下一页</a>
                    <?php endif; ?>
                </div>
            </div>
            <?php endif; ?>
        <?php endif; ?>
<?php live_region_end(); ?>
<?php layout_footer(); ?>
