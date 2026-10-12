<?php
// =============================================================================
// 扩缩容配置 — cluster:config:process:{type} Hash (每进程类型)
//   min_count / max_count / cpu_warn / cpu_full / mem_warn_mb / mem_full_mb
//   / actor_max / weight
// 阈值供资源感知负载均衡 (xLinkManager::GetBestPeer) 与自动扩缩容 Leader 使用。
// =============================================================================
require_once __DIR__ . '/_layout.php';

$r = redis();

// 字段定义: key => [label, 单位/说明]
$FIELDS = [
    'min_count'    => ['最小进程数', ''],
    'max_count'    => ['最大进程数', ''],
    'cpu_warn'     => ['CPU 告警阈值 %', ''],
    'cpu_full'     => ['CPU 满载阈值 %', ''],
    'mem_warn_mb'  => ['内存告警阈值 MB', ''],
    'mem_full_mb'  => ['内存满载阈值 MB', ''],
    'actor_max'    => ['单进程 Actor 上限', ''],
    'weight'       => ['容量权重', '扩缩容容量校验用'],
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
        foreach (array_keys(PROCESS_TYPES) as $type) {
            $row = $cfg[$type] ?? [];
            if (!is_array($row) || empty($row)) continue;
            $key = config_process_key((int)$type);
            $data = [];
            foreach (array_keys($FIELDS) as $f) {
                $data[$f] = trim((string)($row[$f] ?? ''));
            }
            redis_hash_set($r, $key, $data);
            $saved++;
        }
        $notice = "已保存 {$saved} 个类型的扩缩容配置";
    }
}

// 读取当前值
$configs = [];
if ($r !== null) {
    foreach (array_keys(PROCESS_TYPES) as $type) {
        $configs[$type] = redis_hash_get($r, config_process_key((int)$type));
    }
}

layout_header('扩缩容配置', 'settings_scaling');
?>
<?php if ($notice !== ''): ?>
    <div class="alert <?= $noticeOk ? 'alert-success' : 'alert-warning' ?>"><?= esc($notice) ?></div>
<?php endif; ?>

<div class="card">
    <div class="card-header">各类型扩缩容阈值 <span class="dim">cluster:config:process:{type}</span></div>
    <div class="card-body">
        <?php if ($r === null): ?>
            <div class="empty">⚠️ 无法连接 Redis Cluster。</div>
        <?php else: ?>
        <form method="post" class="form" style="max-width:none">
            <div class="table-responsive"><table class="table table-vcenter">
                <thead><tr>
                    <th>类型</th>
                    <?php foreach ($FIELDS as $f => $m): ?>
                        <th><?= esc($m[0]) ?><?= $m[1] !== '' ? '<br><span class="dim">(' . esc($m[1]) . ')</span>' : '' ?></th>
                    <?php endforeach; ?>
                </tr></thead>
                <tbody>
                <?php foreach ($configs as $type => $c): ?>
                    <tr>
                        <td><?= esc(process_type_name((string)$type)) ?> <span class="dim">(<?= $type ?>)</span></td>
                        <?php foreach (array_keys($FIELDS) as $f): ?>
                            <td><input class="form-control" type="number" name="cfg[<?= $type ?>][<?= esc($f) ?>]" value="<?= esc($c[$f] ?? '') ?>" style="width:96px"></td>
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
            说明：<code>cpu_warn/cpu_full</code> 为百分比整数 (0-100)，<code>mem_warn_mb/mem_full_mb</code> 为 MB；
            <code>actor_max</code> 为单进程承载 Actor 上限，<code>weight</code> 用于扩缩容时的容量权重折算。留空表示不覆盖缺省值。
        </div>
        <?php endif; ?>
    </div>
</div>

<?php layout_footer(); ?>
