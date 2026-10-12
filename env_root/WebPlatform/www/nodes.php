<?php
// 进程容器节点状态展示 — 读 Redis 的 cluster:process:*:* 注册数据
// 改造: 加 machine_id 分组 + cpu/内存列 (P1 指标字段)。
require_once __DIR__ . '/_layout.php';

$r = redis();

/**
 * 拉取所有节点 (类型 → 节点列表)
 * @return array<int, list<array<string,string>>>
 */
function fetch_nodes(?RedisCluster $r): array
{
    $result = [];
    if ($r === null) {
        return $result;
    }
    foreach (array_keys(PROCESS_TYPES) as $type) {
        $members = $r->smembers("cluster:process:by_type:{$type}");
        if (!is_array($members)) {
            continue;
        }
        foreach ($members as $id) {
            $hash = $r->hgetall("cluster:process:{$type}:{$id}");
            if (!is_array($hash) || empty($hash)) {
                continue;
            }
            if (!is_online($hash)) {
                continue;   // 只保留在线节点, 过滤掉离线/僵尸条目
            }
            $hash['type'] = (string)$type;
            $result[$type][] = $hash;
        }
    }
    return $result;
}

$nodesByType = fetch_nodes($r);   // 已过滤: 只含在线节点
$online = array_sum(array_map('count', $nodesByType));

layout_header('节点状态', 'nodes');
autorefresh();
live_region_start();
?>
<div class="stat-grid">
    <div class="stat"><div class="k">在线节点</div><div class="v" style="color:var(--online)"><?= $online ?></div></div>
</div>

<?php if ($r === null): ?>
    <div class="empty">⚠️ 无法连接 Redis，请确认 Redis 已启动且 RealServer 已注册节点。</div>
<?php elseif ($online === 0): ?>
    <div class="empty">暂无在线节点 —— 启动 RealServer 后，它会向 Redis 写入 <code>cluster:process:*:*</code>。</div>
<?php else: ?>
    <?php foreach ($nodesByType as $type => $nodes): ?>
        <div class="card">
            <div class="card-header"><?= esc(process_type_name((string)$type)) ?> <span class="dim">(type=<?= $type ?>)</span></div>
            <div class="card-body">
                <div class="table-responsive"><table class="table table-vcenter">
                    <thead>
                        <tr>
                            <th>进程ID</th><th>名称</th><th>机器</th><th>IP</th><th>端口</th>
                            <th>状态</th><th>Actor</th><th>Lua版本</th><th>热更版本</th><th>CPU</th><th>内存</th><th>心跳</th><th>验证Key</th>
                        </tr>
                    </thead>
                    <tbody>
                    <?php foreach ($nodes as $n): ?>
                        <tr class="<?= is_online($n) ? 'online' : 'offline' ?>">
                            <td class="mono"><?= esc($n['id'] ?? '') ?></td>
                            <td><?= esc($n['name'] ?? '') ?></td>
                            <td class="mono"><?= esc($n['machine_id'] ?? '0') ?></td>
                            <td class="mono"><?= esc($n['ip'] ?? $n['host'] ?? '') ?></td>
                            <td class="mono"><?= esc($n['port'] ?? '') ?></td>
                            <td><span class="badge <?= is_online($n) ? 'text-bg-success' : 'text-bg-secondary' ?>"><?= esc(state_label($n['state'] ?? '')) ?></span></td>
                            <td><?= esc($n['load'] ?? '0') ?></td>
                            <td class="mono"><?= esc($n['lua_version'] ?? '—') ?></td>
                            <td class="mono small">
                                <?php
                                $hv = $n['hotfix_versions'] ?? '';
                                $hs = $n['hotfix_status'] ?? '';
                                $he = $n['hotfix_error'] ?? '';
                                if ($hv === '') { echo '<span class="dim">—</span>'; } else { echo esc($hv); }
                                if ($hs !== '' && $hs !== 'ok' && $hs !== 'precheck_ok') {
                                    echo ' <span class="badge text-bg-warning" title="' . esc($he) . '">' . esc($hs) . '</span>';
                                }
                                ?>
                            </td>
                            <td class="bar-cell">
                                <?php $cpu = (int)($n['cpu_percent'] ?? 0); ?>
                                <div class="bar"><span class="<?= $cpu >= 90 ? 'full' : ($cpu >= 70 ? 'high' : '') ?>" style="width:<?= min(100, $cpu) ?>%"></span></div>
                                <div class="pct"><?= $cpu ?>%</div>
                            </td>
                            <td><?= esc($n['mem_rss_mb'] ?? '') ?> MB</td>
                            <td><?= is_online($n) ? '在线' : '离线' ?></td>
                            <td class="mono"><?= esc($n['authkey'] ?? '') ?></td>
                        </tr>
                    <?php endforeach; ?>
                    </tbody>
                </table></div>
            </div>
        </div>
    <?php endforeach; ?>
<?php endif; ?>

<?php live_region_end(); ?>
<?php layout_footer(); ?>
