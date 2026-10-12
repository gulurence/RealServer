<?php
// 战斗战报详情 — 解码 battle:replay:{id} (逐帧输入) + battle:result:{id} (摘要)
require_once __DIR__ . '/_layout.php';

function fmt_time(?string $ts): string
{
    $i = (int)$ts;
    return $i > 0 ? date('Y-m-d H:i:s', $i) : '-';
}

// 移动方向编码 → 中文名 (与 battlecore 8 方向编码一致)
const DIR_NAMES = [0 => '停止', 1 => '上', 2 => '右上', 3 => '右', 4 => '右下', 5 => '下', 6 => '左下', 7 => '左', 8 => '左上'];

$battleId = $_GET['id'] ?? '';
$r = redis();

$summary = null;
$replayWin = '-'; $replayHash = '-'; $frames = [];   // frames[frameNo] = list<{pid,move,shootDx,shootDy,skill,fire,gun}>
$roster = '-';

if ($r !== null && $battleId !== '') {
    $rawResult = $r->get("battle:result:{$battleId}");
    if (is_string($rawResult)) {
        $p = explode('|', $rawResult);
        $summary = [
            'win' => $p[0] ?? '-', 'hash' => $p[1] ?? '-', 'frames' => $p[2] ?? '-',
            'ts' => $p[3] ?? '0', 'players' => $p[4] ?? '-', 'roster' => $p[5] ?? '-',
        ];
    }

    $rawReplay = $r->get("battle:replay:{$battleId}");
    if (is_string($rawReplay) && $rawReplay !== '') {
        $seg = explode('|', $rawReplay);
        $replayWin  = $seg[0] ?? '-';
        $replayHash = $seg[1] ?? '-';
        // frame_i 从 index 2 起: "pid,move,shoot_dx,shoot_dy,skill,fire,gunType;pid,..."
        for ($f = 2; $f < count($seg); ++$f) {
            $frameNo = $f - 1;   // 1-based 帧号
            $frameInputs = [];
            foreach (explode(';', $seg[$f]) as $in) {
                if ($in === '') continue;
                $c = explode(',', $in);
                $frameInputs[] = [
                    'pid'     => $c[0] ?? '-',
                    'move'    => $c[1] ?? '-',
                    'shootDx' => $c[2] ?? '0',
                    'shootDy' => $c[3] ?? '0',
                    'skill'   => $c[4] ?? '-',
                    'fire'    => ($c[5] ?? '0') === '1',
                    'gun'     => $c[6] ?? '0',
                ];
            }
            $frames[$frameNo] = $frameInputs;
        }
    }

    $rosterRaw = $r->get("battle:roster:{$battleId}");
    if (is_string($rosterRaw) && $rosterRaw !== '') {
        $roster = $rosterRaw;
    }
}

$win = $summary['win'] ?? $replayWin;
?>
<?php layout_header('战报详情 #' . esc($battleId), 'battles'); ?>
<?php autorefresh(); ?>
<?php live_region_start(); ?>
        <div class="summary">
            <a href="battles.php">← 返回列表</a> &nbsp;|&nbsp;
            战斗 <b class="mono">#<?= esc($battleId) ?></b>
        </div>

        <?php if ($r === null): ?>
            <div class="empty">⚠️ 无法连接 Redis。</div>
        <?php elseif ($summary === null && $replayHash === '-'): ?>
            <div class="empty">未找到该战斗的战报数据（<code>battle:result:<?= esc($battleId) ?></code>）。</div>
        <?php else: ?>
            <section class="node-group">
                <h2>战报摘要</h2>
                <table class="table table-vcenter">
                    <tbody>
                        <tr><th>胜负 (win side)</th><td class="mono"><?= esc($win) ?></td></tr>
                        <tr><th>最终状态哈希</th><td class="mono"><?= esc($summary['hash'] ?? $replayHash) ?></td></tr>
                        <tr><th>回放哈希</th><td class="mono"><?= esc($replayHash) ?></td></tr>
                        <tr><th>总帧数</th><td><?= esc($summary['frames'] ?? count($frames)) ?></td></tr>
                        <tr><th>参与人数</th><td><?= esc($summary['players'] ?? '-') ?></td></tr>
                        <tr><th>花名册 (pid 顺序)</th><td class="mono"><?= esc($roster) ?></td></tr>
                        <tr><th>结算时间</th><td><?= esc(fmt_time($summary['ts'] ?? '0')) ?></td></tr>
                    </tbody>
                </table>
            </section>

            <section class="node-group">
                <h2>逐帧输入回放 <span class="dim">（共 <?= count($frames) ?> 帧）</span></h2>
                <?php if (empty($frames)): ?>
                    <div class="empty">无逐帧录制数据。</div>
                <?php else: ?>
                    <table class="table table-vcenter">
                        <thead>
                            <tr><th>帧</th><th>输入 (pid: move/射向dx,dy/skill/fire/枪)</th></tr>
                        </thead>
                        <tbody>
                        <?php foreach ($frames as $frameNo => $inputs): ?>
                            <tr>
                                <td class="mono"><?= $frameNo ?></td>
                                <td class="mono">
                                <?php foreach ($inputs as $in): ?>
                                    <span title="pid=<?= esc($in['pid']) ?>">
                                        p<?= esc($in['pid']) ?>: <?= esc(DIR_NAMES[(int)$in['move']] ?? $in['move']) ?>/<?= esc($in['shootDx']) ?>,<?= esc($in['shootDy']) ?>/<?= esc($in['skill']) ?>/<?= $in['fire'] ? '火' : '·' ?>/<?= esc($in['gun']) ?>
                                    </span>;
                                <?php endforeach; ?>
                                </td>
                            </tr>
                        <?php endforeach; ?>
                        </tbody>
                    </table>
                <?php endif; ?>
            </section>
        <?php endif; ?>
<?php live_region_end(); ?>
<?php layout_footer(); ?>
