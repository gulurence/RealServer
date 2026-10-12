<?php
// =============================================================================
// 节点配置 — cluster:config:node:{machineId} Hash (每机器)
//   enable / capacity_cap / role
// capacity_cap = 本机允许的最大进程总量 (缺省由 NodeManager 按 cpu_cores 折算)。
// =============================================================================
require_once __DIR__ . '/_layout.php';

$r = redis();

$FIELDS = [
    'enable'       => ['启用', 'select', ['1' => '启用', '0' => '禁用']],
    'capacity_cap' => ['进程总量上限', 'number', null],
    'role'         => ['角色', 'text', null],
];

$notice = '';
$noticeOk = true;
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    if ($r === null) {
        $notice = '无法连接 Redis，配置未保存';
        $noticeOk = false;
    } else {
        $cfg = $_POST['cfg'] ?? [];
        $saved = 0;
        foreach ($cfg as $machineId => $row) {
            if (!is_array($row)) continue;
            $key = config_node_key((int)$machineId);
            $data = [];
            foreach (array_keys($FIELDS) as $f) {
                $data[$f] = trim((string)($row[$f] ?? ''));
            }
            redis_hash_set($r, $key, $data);
            $saved++;
        }
        $notice = "已保存 {$saved} 台机器的节点配置";
    }
}

$machines = fetch_machines($r);
$configs = [];
if ($r !== null) {
    foreach (array_keys($machines) as $machineId) {
        $configs[$machineId] = redis_hash_get($r, config_node_key((int)$machineId));
    }
}

layout_header('节点配置', 'settings_nodes');
?>
<?php if ($notice !== ''): ?>
    <div class="alert <?= $noticeOk ? 'alert-success' : 'alert-warning' ?>"><?= esc($notice) ?></div>
<?php endif; ?>

<div class="card">
    <div class="card-header">各机器节点配置 <span class="dim">cluster:config:node:{machineId}</span></div>
    <div class="card-body">
        <?php if ($r === null): ?>
            <div class="empty">⚠️ 无法连接 Redis Cluster。</div>
        <?php elseif (empty($machines)): ?>
            <div class="empty">暂无注册机器 —— 启动 NodeManager 后在此配置容量上限。</div>
        <?php else: ?>
        <form method="post" class="form" style="max-width:none">
            <div class="table-responsive"><table class="table table-vcenter">
                <thead><tr>
                    <th>machine_id</th><th>主机名</th><th>CPU 核数</th><th>内存总量</th>
                    <?php foreach ($FIELDS as $f => $m): ?>
                        <th><?= esc($m[0]) ?></th>
                    <?php endforeach; ?>
                </tr></thead>
                <tbody>
                <?php foreach ($machines as $machineId => $m):
                    $c = $configs[$machineId] ?? [];
                ?>
                    <tr>
                        <td class="mono"><?= esc((string)$machineId) ?></td>
                        <td><?= esc($m['hostname'] ?? '') ?></td>
                        <td><?= esc($m['cpu_cores'] ?? '') ?></td>
                        <td><?= esc($m['mem_total_mb'] ?? '') ?> MB</td>
                        <?php foreach ($FIELDS as $f => $meta): ?>
                            <td>
                                <?php if ($meta[1] === 'select'): ?>
                                    <select class="form-select" name="cfg[<?= $machineId ?>][<?= esc($f) ?>]">
                                        <?php foreach ($meta[2] as $ov => $ol): ?>
                                            <option value="<?= esc((string)$ov) ?>" <?= (string)($c[$f] ?? '') === (string)$ov ? 'selected' : '' ?>><?= esc((string)$ol) ?></option>
                                        <?php endforeach; ?>
                                    </select>
                                <?php else: ?>
                                    <input type="<?= esc($meta[1]) ?>" name="cfg[<?= $machineId ?>][<?= esc($f) ?>]" value="<?= esc($c[$f] ?? '') ?>" style="width:110px">
                                <?php endif; ?>
                            </td>
                        <?php endforeach; ?>
                    </tr>
                <?php endforeach; ?>
                </tbody>
            </table></div>
            <div class="actions">
                <button class="btn btn-primary" type="submit">保存全部</button>
            </div>
        </form>
        <div class="dim" style="margin-top:14px">
            说明：<code>capacity_cap</code> 留空时 NodeManager 按 <code>cpu_cores × 4</code> 折算缺省上限；
            <code>role</code> 用于标记机器用途（如 gate/game/mixed），供自动扩缩容选机参考。
        </div>
        <?php endif; ?>
    </div>
</div>

<?php layout_footer(); ?>
