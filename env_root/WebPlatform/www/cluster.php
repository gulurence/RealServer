<?php
// =============================================================================
// 集群控制页 — 机器列表 + 各类型进程数 + 手动扩缩容 + 扩缩容事件流
//
// 期望数: 读/写 cluster:desired:{type}.count → NodeManager 每 2s 收敛。
// 存活数: 读 cluster:process:by_type:{type} + cluster:process:{type}:{id},
//         按心跳新鲜(≤15s)判定真正存活；心跳过期的成员视为「僵尸」并单独显示。
// 页面每 5s 自动刷新 (可暂停), 实时反映 Redis 中的集群状态。
// =============================================================================
require_once __DIR__ . '/_layout.php';

$r = redis();

// ---------- 单类型进程行统计 ----------
// 存活 = 心跳新鲜 (真实在跑)；僵尸 = 注册在案但心跳过期 (崩溃/被强杀未注销)。
function process_row(?RedisCluster $r, int $type): array
{
    $row = [
        'desired' => 0, 'alive' => 0, 'stale' => 0, 'registered' => 0,
        'cpu' => 0, 'mem' => 0, 'actors' => 0,
    ];
    if ($r === null) return $row;

    $row['desired'] = (int)($r->hget(desired_key($type), 'count') ?? 0);

    $members = $r->smembers("cluster:process:by_type:{$type}");
    if (!is_array($members)) return $row;

    foreach ($members as $id) {
        $h = $r->hgetall("cluster:process:{$type}:{$id}");
        if (!is_array($h) || empty($h)) continue;
        $row['registered']++;
        if (is_online($h)) {
            $row['alive']++;
            $row['cpu']    = max($row['cpu'], (int)($h['cpu_percent'] ?? 0));
            $row['mem']    = max($row['mem'], (int)($h['mem_percent'] ?? 0));
            $row['actors'] += (int)($h['load'] ?? 0);
        } else {
            $row['stale']++;
        }
    }
    return $row;
}

// ---------- 清理僵尸进程条目 (集合成员 + 详情 Hash) ----------
function cleanup_stale_processes(?RedisCluster $r, int $type): int
{
    if ($r === null) return 0;
    $setKey  = "cluster:process:by_type:{$type}";
    $members = $r->smembers($setKey);
    if (!is_array($members)) return 0;

    $removed = 0;
    foreach ($members as $id) {
        $h = $r->hgetall("cluster:process:{$type}:{$id}");
        $stale = (!is_array($h) || empty($h)) || !is_online($h);
        if (!$stale) continue;
        $r->srem($setKey, $id);
        $r->del("cluster:process:{$type}:{$id}");
        $removed++;
    }
    return $removed;
}

// ---------- 动作处理 ----------
$notice = '';
$noticeOk = true;
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $action = post_str('action');
    $type   = post_int('type');
    if ($r === null) {
        $notice = '无法连接 Redis Cluster，未执行操作';
        $noticeOk = false;
    } elseif (!array_key_exists($type, PROCESS_TYPES)) {
        $notice = '无效的进程类型';
        $noticeOk = false;
    } elseif ($action === 'scale') {
        $delta = post_int('delta', 0);
        $count = post_str('count', '');
        $cur = (int)($r->hget(desired_key($type), 'count') ?? 0);
        if ($count !== '') {
            $new = max(0, (int)$count);
        } else {
            $new = max(0, $cur + $delta);
        }
        $r->hset(desired_key($type), 'count', (string)$new);
        $r->hset(desired_key($type), 'mode', 'manual');
        $notice = "已设置 " . process_type_name((string)$type)
            . " (type={$type}) 期望进程数 = {$new}（NodeManager 每 2s 收敛）";
    } elseif ($action === 'sync') {
        $alive = process_row($r, $type)['alive'];
        $r->hset(desired_key($type), 'count', (string)$alive);
        $r->hset(desired_key($type), 'mode', 'manual');
        $notice = "已同步 " . process_type_name((string)$type)
            . " (type={$type}) 期望数 = 存活数 {$alive}";
    } elseif ($action === 'cleanup') {
        $removed = cleanup_stale_processes($r, $type);
        $notice = "已清理 " . process_type_name((string)$type)
            . " (type={$type}) 僵尸进程条目 {$removed} 个";
    } else {
        $notice = '未知操作';
        $noticeOk = false;
    }
}

// ---------- 机器列表 ----------
$machines = fetch_machines($r);

// ---------- 各类型进程数 + 期望数 ----------
$typeRows = [];
if ($r !== null) {
    foreach (array_keys(PROCESS_TYPES) as $type) {
        $typeRows[$type] = process_row($r, (int)$type);
    }
}

// ---------- 事件流 ----------
$alerts = fetch_alerts($r, 40);

layout_header('集群控制', 'cluster');
autorefresh(1, '立即刷新');
live_region_start();
?>
<?php if ($notice !== ''): ?>
    <div class="alert <?= $noticeOk ? 'alert-success' : 'alert-warning' ?>"><?= esc($notice) ?></div>
<?php endif; ?>

<?php if ($r === null): ?>
    <div class="empty">⚠️ 无法连接 Redis Cluster（种子 <?= esc(REDIS_SEEDS[0]) ?>）。</div>
<?php else: ?>
    <!-- 机器列表 -->
    <div class="card">
        <div class="card-header">物理机器 <span class="dim">cluster:node:* / cluster:node:all</span></div>
        <div class="card-body">
            <?php if (empty($machines)): ?>
                <div class="empty">暂无注册机器 —— 启动 NodeManager 后会注册本机。</div>
            <?php else: ?>
                <div class="table-responsive"><table class="table table-vcenter">
                    <thead><tr>
                        <th>machine_id</th><th>主机名</th><th>IP</th><th>OS</th>
                        <th>CPU 核数</th><th>内存总量</th><th>状态</th><th>心跳</th>
                    </tr></thead>
                    <tbody>
                    <?php foreach ($machines as $id => $m):
                        $hb = (int)($m['heartbeat'] ?? 0);
                        $online = $hb > 0 && (time() - $hb) <= 30;
                    ?>
                        <tr class="<?= $online ? 'online' : 'offline' ?>">
                            <td class="mono"><?= esc($m['machine_id'] ?? $id) ?></td>
                            <td><?= esc($m['hostname'] ?? '') ?></td>
                            <td class="mono"><?= esc($m['ip'] ?? '') ?></td>
                            <td><?= esc($m['os'] ?? '') ?></td>
                            <td><?= esc($m['cpu_cores'] ?? '') ?></td>
                            <td><?= esc($m['mem_total_mb'] ?? '') ?> MB</td>
                            <td><span class="badge <?= $online ? 'text-bg-success' : 'text-bg-secondary' ?>"><?= $online ? '在线' : '离线' ?></span></td>
                            <td class="mono"><?= esc($m['heartbeat'] ?? '') ?></td>
                        </tr>
                    <?php endforeach; ?>
                    </tbody>
                </table></div>
            <?php endif; ?>
        </div>
    </div>

    <!-- 各类型进程 + 扩缩容 -->
    <div class="card">
        <div class="card-header">进程类型 + 手动扩缩容 <span class="dim">写 cluster:desired:{type}.count</span></div>
        <div class="card-body">
            <div class="table-responsive"><table class="table table-vcenter">
                <thead><tr>
                    <th>类型</th><th>期望数</th><th>存活</th><th>僵尸</th>
                    <th>峰值 CPU</th><th>峰值内存</th><th>Actor 总数</th><th>操作</th>
                </tr></thead>
                <tbody>
                <?php foreach ($typeRows as $type => $row): ?>
                    <tr>
                        <td><?= esc(process_type_name((string)$type)) ?> <span class="dim">(<?= $type ?>)</span></td>
                        <td><b><?= $row['desired'] ?></b></td>
                        <td><?= $row['alive'] ?></td>
                        <td>
                            <?php if ($row['stale'] > 0): ?>
                                <span class="badge text-bg-warning" title="注册在案但心跳过期的进程，可点「清理僵尸」移除"><?= $row['stale'] ?></span>
                            <?php else: ?>0<?php endif; ?>
                        </td>
                        <td class="bar-cell">
                            <div class="bar"><span class="<?= $row['cpu'] >= 90 ? 'full' : ($row['cpu'] >= 70 ? 'high' : '') ?>" style="width:<?= min(100, $row['cpu']) ?>%"></span></div>
                            <div class="pct"><?= $row['cpu'] ?>%</div>
                        </td>
                        <td class="bar-cell">
                            <div class="bar"><span class="<?= $row['mem'] >= 90 ? 'full' : ($row['mem'] >= 70 ? 'high' : '') ?>" style="width:<?= min(100, $row['mem']) ?>%"></span></div>
                            <div class="pct"><?= $row['mem'] ?>%</div>
                        </td>
                        <td><?= $row['actors'] ?></td>
                        <td>
                            <form method="post" style="display:inline">
                                <input type="hidden" name="action" value="scale">
                                <input type="hidden" name="type" value="<?= $type ?>">
                                <button class="btn btn-sm" name="delta" value="-1" type="submit">−1</button>
                                <button class="btn btn-sm" name="delta" value="1" type="submit">+1</button>
                            </form>
                            <form method="post" style="display:inline;margin-left:6px">
                                <input type="hidden" name="action" value="scale">
                                <input type="hidden" name="type" value="<?= $type ?>">
                                <input class="form-control" type="number" name="count" min="0" placeholder="N" style="width:60px">
                                <button class="btn btn-sm btn-primary" type="submit">设</button>
                            </form>
                            <form method="post" style="display:inline;margin-left:6px">
                                <input type="hidden" name="action" value="sync">
                                <input type="hidden" name="type" value="<?= $type ?>">
                                <button class="btn btn-sm btn-outline-success" type="submit" title="把期望数改为当前存活数">期望=存活</button>
                            </form>
                            <?php if ($row['stale'] > 0): ?>
                            <form method="post" style="display:inline;margin-left:6px">
                                <input type="hidden" name="action" value="cleanup">
                                <input type="hidden" name="type" value="<?= $type ?>">
                                <button class="btn btn-sm btn-outline-danger" type="submit" title="移除心跳过期的僵尸进程条目">清理僵尸</button>
                            </form>
                            <?php endif; ?>
                        </td>
                    </tr>
                <?php endforeach; ?>
                </tbody>
            </table></div>
            <div class="dim" style="margin-top:12px">
                提示：手动扩缩容写期望数，由每台机器的 NodeManager 每 2 秒收敛；<code>−1</code> 会停止一个多余进程。
                存活数按心跳新鲜(≤15s)统计；「僵尸」是崩溃/被强杀后未注销的残留条目，可点「清理僵尸」清除，
                或用「期望=存活」把期望数收敛到真实运行数。
            </div>
        </div>
    </div>

    <!-- 事件流 -->
    <div class="card">
        <div class="card-header">扩缩容 / 告警事件 <span class="dim">cluster:alert</span></div>
        <div class="card-body">
            <?php if (empty($alerts)): ?>
                <div class="empty">暂无事件。</div>
            <?php else: ?>
                <div class="event-list">
                <?php foreach ($alerts as $line): ?>
                    <div class="event-item"><span class="t"><?= esc(substr((string)$line, 0, 20)) ?></span><span class="msg"><?= esc((string)$line) ?></span></div>
                <?php endforeach; ?>
                </div>
            <?php endif; ?>
        </div>
    </div>
<?php endif; ?>
<?php live_region_end(); ?>
<?php layout_footer(); ?>
