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
?>
<?php layout_header('战斗战报', 'battles'); ?>
<?php autorefresh(); ?>
<?php live_region_start(); ?>
        <div class="summary">
            战斗总数：<b><?= count($battles) ?></b>
            <span class="dim">（按结算时间倒序，每 5 秒自动刷新）</span>
        </div>

        <?php if ($r === null): ?>
            <div class="empty">⚠️ 无法连接 Redis，请确认 Redis 已启动且战斗已结算。</div>
        <?php elseif (empty($battles)): ?>
            <div class="empty">暂无战斗战报 —— 战斗结算后会自动写入 <code>battle:index</code>。</div>
        <?php else: ?>
            <table class="table table-vcenter">
                <thead>
                    <tr>
                        <th>战斗ID</th><th>胜负</th><th>最终哈希</th><th>帧数</th>
                        <th>人数</th><th>结算时间</th><th></th>
                    </tr>
                </thead>
                <tbody>
                <?php foreach ($battles as $b): ?>
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
        <?php endif; ?>
<?php live_region_end(); ?>
<?php layout_footer(); ?>
