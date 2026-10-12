<?php
// Actor 信息显示界面 — 单个 Actor 的详情 + 监控上报
//
// 打开本页后, 页面每 1 秒自动刷新 (autorefresh), 每次刷新都会重新向 Redis 发布一条
// 订阅请求 (cluster:actor:report:req, JSON {"type":N,"id":M}), 告知服务器该详情页仍打开。
// 服务器侧 CActorReportMgr 收到订阅即持续把该 Actor 的上报结构写到
// cluster:actor:report:{type}:{id} (JSON String, 15s TTL); 页面关闭后 ~20s 无订阅,
// 服务器自动停止上报。
require_once __DIR__ . '/_layout.php';

$type = (int)($_GET['type'] ?? 0);
$id   = (int)($_GET['id'] ?? 0);

$r = redis();

// 每次请求 (含 ?partial=1 自动刷新) 都发布订阅请求, 续订服务器上报标记
if ($r !== null && $type > 0) {
    try {
        $r->publish('cluster:actor:report:req', json_encode(['type' => $type, 'id' => $id]));
    } catch (Throwable $e) {
        // Redis 不可用时静默, 页面仍展示已有基础信息
    }
}

// 基础信息 (来自 Actor 注册表, 与 actors.php 同源)
$base = [];
if ($r !== null && $type > 0) {
    try {
        $h = $r->hgetall("cluster:actor:{$type}:{$id}");
        if (is_array($h)) $base = $h;
    } catch (Throwable $e) {}
}

// 上报数据 (JSON String, 15s TTL)
$report = null;
if ($r !== null && $type > 0) {
    try {
        $raw = $r->get("cluster:actor:report:{$type}:{$id}");
        if (is_string($raw) && $raw !== '') {
            $report = json_decode($raw, true);
        }
    } catch (Throwable $e) {}
}

$typeName = ACTOR_TYPES[$type] ?? "Type{$type}";

/** Unix 秒 → 'Y-m-d H:i:s' (无/非法为 '—') */
function ts_to_str($v): string
{
    $t = (int)$v;
    return $t > 0 ? date('Y-m-d H:i:s', $t) : '—';
}

/** 战斗胜负编码 (BattleGame::CheckWin): 0=继续, -1=平局, >0=第 N 位存活者胜 */
function battle_win_label($v): string
{
    $v = (int)$v;
    if ($v > 0) return "玩家{$v}胜";
    if ($v < 0) return '平局';
    return '进行中';
}
?>
<?php layout_header("Actor 详情 — {$typeName} #{$id}", 'actors'); ?>
<?php autorefresh(1, '立即刷新'); ?>
<?php live_region_start(); ?>
        <div class="summary">
            Actor 详情：<b><?= esc($typeName) ?></b>
            <span class="dim">（type=<?= $type ?> / 业务ID=<?= $id ?>）</span>
            <span class="dim">—— 每 1 秒自动刷新并续订上报</span>
        </div>

        <?php if ($r === null): ?>
            <div class="empty">⚠️ 无法连接 Redis。</div>
        <?php else: ?>
            <section class="node-group">
                <h2>基础信息</h2>
                <table class="table table-vcenter">
                    <tbody>
                        <tr><th style="width:180px">Actor ID (全局)</th><td class="mono"><?= esc($base['gid'] ?? $report['global_id'] ?? '—') ?></td></tr>
                        <tr><th>业务ID</th><td class="mono"><?= esc($base['id'] ?? (string)$id) ?></td></tr>
                        <tr><th>名称</th><td><?= esc($base['name'] ?? $report['name'] ?? '—') ?></td></tr>
                        <tr><th>承载进程</th><td><?= esc(PROCESS_TYPES[(int)($base['ptype'] ?? 0)] ?? $base['ptype'] ?? '—') ?></td></tr>
                        <tr><th>进程ID</th><td class="mono"><?= esc($base['pid'] ?? '—') ?></td></tr>
                        <tr><th>状态</th><td><span class="badge"><?= ($base['state'] ?? '') === '1' ? 'active' : esc($base['state'] ?? '—') ?></span></td></tr>
                        <tr><th>创建时间</th><td class="mono"><?= esc(ts_to_str($base['created'] ?? $report['create_time'] ?? null)) ?></td></tr>
                    </tbody>
                </table>
            </section>

            <section class="node-group">
                <h2>监控上报 <span class="dim">（服务器实时采集，15 秒内有效）</span></h2>
                <?php if ($report === null || !is_array($report)): ?>
                    <div class="empty">暂无上报数据 —— 首次打开需等待服务器下一轮上报（≤ 1 秒）。</div>
                <?php else: ?>
                <table class="table table-vcenter">
                    <tbody>
                        <tr><th style="width:180px">全局 ID</th><td class="mono"><?= esc((string)($report['global_id'] ?? '—')) ?></td></tr>
                        <tr><th>负载分值</th><td><?= esc((string)($report['load_score'] ?? '—')) ?></td></tr>
                        <tr><th>状态</th><td><span class="badge"><?= ($report['state'] ?? 0) === 1 ? 'active' : esc((string)($report['state'] ?? '—')) ?></span></td></tr>
                        <tr><th>归属 ActorID</th><td class="mono"><?= esc((string)($report['owner_id'] ?? '—')) ?></td></tr>
                        <tr><th>创建时间</th><td class="mono"><?= esc(ts_to_str($report['create_time'] ?? null)) ?></td></tr>
                        <tr><th>最近上报</th><td class="mono"><?= esc(ts_to_str($report['reported_at'] ?? null)) ?></td></tr>
                    </tbody>
                </table>
                <?php endif; ?>
            </section>

            <?php if ($type === 203 && isset($report['battle']) && is_array($report['battle'])): $b = $report['battle']; ?>
            <section class="node-group">
                <h2>战斗状态 <span class="dim">（BattleGame / HeroState 确定性状态）</span></h2>
                <table class="table table-vcenter">
                    <tbody>
                        <tr><th style="width:180px">帧号</th><td class="mono"><?= esc((string)($b['frame'] ?? '—')) ?></td></tr>
                        <tr><th>状态哈希</th><td class="mono"><?= esc((string)($b['hash'] ?? '—')) ?></td></tr>
                        <tr><th>胜负</th><td><span class="badge"><?= esc(battle_win_label($b['win'] ?? 0)) ?></span></td></tr>
                        <tr><th>波次</th><td><?= esc((string)($b['wave'] ?? '—')) ?>（显示 <?= esc((string)($b['wave_show'] ?? '—')) ?> / 计时 <?= esc((string)($b['wave_timer'] ?? '—')) ?>）</td></tr>
                        <tr><th>全场清空</th><td><?= !empty($b['all_cleared']) ? '是' : '否' ?></td></tr>
                        <tr><th>实体数</th><td class="mono">英雄 <?= esc((string)($b['counts']['hero'] ?? '—')) ?> · 小怪 <?= esc((string)($b['counts']['sp01'] ?? '—')) ?> · 子弹 <?= esc((string)($b['counts']['bullet'] ?? '—')) ?> · 炮台 <?= esc((string)($b['counts']['turret'] ?? '—')) ?> · 道具 <?= esc((string)($b['counts']['prop'] ?? '—')) ?></td></tr>
                    </tbody>
                </table>

                <?php if (!empty($b['heroes']) && is_array($b['heroes'])): ?>
                <h3 class="doc-h3">英雄状态</h3>
                <table class="table table-vcenter">
                    <thead>
                        <tr><th>玩家ID</th><th>位置(x,y)</th><th>HP</th><th>存活</th><th>枪</th><th>击杀</th><th>炸弹引信</th><th>移动</th><th>动作</th><th>最近开火帧</th></tr>
                    </thead>
                    <tbody>
                        <?php foreach ($b['heroes'] as $h): ?>
                        <tr>
                            <td class="mono"><?= esc((string)($h['id'] ?? '—')) ?></td>
                            <td class="mono"><?= esc((string)($h['x'] ?? '—')) ?>, <?= esc((string)($h['y'] ?? '—')) ?></td>
                            <td><?= esc((string)($h['hp'] ?? '—')) ?> / <?= esc((string)($h['max_hp'] ?? '—')) ?></td>
                            <td><?= !empty($h['alive']) ? '存活' : '阵亡' ?></td>
                            <td><?= esc((string)($h['gun'] ?? '—')) ?></td>
                            <td><?= esc((string)($h['kills'] ?? '—')) ?></td>
                            <td><?= esc((string)($h['bomb'] ?? '—')) ?></td>
                            <td><?= esc((string)($h['move'] ?? '—')) ?></td>
                            <td><?= esc((string)($h['action'] ?? '—')) ?></td>
                            <td class="mono"><?= esc((string)($h['last_shoot'] ?? '—')) ?></td>
                        </tr>
                        <?php endforeach; ?>
                    </tbody>
                </table>
                <?php endif; ?>

                <?php if (!empty($b['monsters']) && is_array($b['monsters'])): ?>
                <h3 class="doc-h3">场景对象 — 怪物</h3>
                <table class="table table-vcenter">
                    <thead>
                        <tr><th>实体ID</th><th>类别</th><th>坐标(x,y)</th><th>HP</th><th>攻击</th><th>动作</th></tr>
                    </thead>
                    <tbody>
                        <?php foreach ($b['monsters'] as $m): ?>
                        <tr>
                            <td class="mono"><?= esc((string)($m['id'] ?? '—')) ?></td>
                            <td class="mono"><?= esc((string)($m['cls'] ?? '—')) ?></td>
                            <td class="mono"><?= esc((string)($m['x'] ?? '—')) ?>, <?= esc((string)($m['y'] ?? '—')) ?></td>
                            <td><?= esc((string)($m['hp'] ?? '—')) ?> / <?= esc((string)($m['max_hp'] ?? '—')) ?></td>
                            <td><?= esc((string)($m['atk'] ?? '—')) ?></td>
                            <td><?= esc((string)($m['action'] ?? '—')) ?></td>
                        </tr>
                        <?php endforeach; ?>
                    </tbody>
                </table>
                <?php endif; ?>

                <?php if (!empty($b['turrets']) && is_array($b['turrets'])): ?>
                <h3 class="doc-h3">场景对象 — 炮台</h3>
                <table class="table table-vcenter">
                    <thead>
                        <tr><th>实体ID</th><th>坐标(x,y)</th><th>HP</th><th>攻击</th><th>激活</th></tr>
                    </thead>
                    <tbody>
                        <?php foreach ($b['turrets'] as $t): ?>
                        <tr>
                            <td class="mono"><?= esc((string)($t['id'] ?? '—')) ?></td>
                            <td class="mono"><?= esc((string)($t['x'] ?? '—')) ?>, <?= esc((string)($t['y'] ?? '—')) ?></td>
                            <td><?= esc((string)($t['hp'] ?? '—')) ?> / <?= esc((string)($t['max_hp'] ?? '—')) ?></td>
                            <td><?= esc((string)($t['atk'] ?? '—')) ?></td>
                            <td><?= !empty($t['active']) ? '是' : '否' ?></td>
                        </tr>
                        <?php endforeach; ?>
                    </tbody>
                </table>
                <?php endif; ?>
            </section>
            <?php endif; ?>
        <?php endif; ?>
<?php live_region_end(); ?>
<?php layout_footer(); ?>
