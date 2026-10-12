<?php
// GM 命令控制台 — 向服务器框架层与 Actor 发送命令 (K8s 风格 kubectl exec)
//
// 命令方向: PUBLISH 到 cluster:gm:all / cluster:gm:type:{t} / cluster:gm:pid:{p}
// 应答方向: 读 Redis 列表 cluster:gm:replies (CGmCommandMgr::LogReply RPUSH 封顶 1000)
require_once __DIR__ . '/_layout.php';

const GM_REPLY_KEY = 'cluster:gm:replies';

$r = redis();
$sent = false;
$notice = '';

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $cmd = trim((string)($_POST['cmd'] ?? ''));
    $target = (string)($_POST['target'] ?? 'all');

    if ($cmd === '') {
        $notice = '命令不能为空';
    } elseif ($r === null) {
        $notice = '无法连接 Redis，命令未发送';
    } else {
        $channel = 'cluster:gm:all';
        if ($target === 'type' && !empty($_POST['type'])) {
            $channel = 'cluster:gm:type:' . (int)$_POST['type'];
        } elseif ($target === 'pid' && !empty($_POST['pid'])) {
            $channel = 'cluster:gm:pid:' . (int)$_POST['pid'];
        }
        $n = $r->publish($channel, $cmd);
        $sent = true;
        $notice = "已向 <code>{$channel}</code> 发布命令，收到订阅进程数：{$n}";
    }
}

$replies = [];
if ($r !== null) {
    $raw = $r->lrange(GM_REPLY_KEY, 0, -1);
    if (is_array($raw)) {
        $replies = array_reverse($raw);   // 最新在前
    }
}
?>
<?php layout_header('GM 控制台', 'gm'); ?>
        <section class="node-group">
            <h2>发送 GM 命令</h2>
            <?php if ($notice !== ''): ?>
                <div class="summary"><?= $notice ?></div>
            <?php endif; ?>
            <form method="post" class="gm-form">
                <label>目标：
                    <select class="form-select" name="target" id="gm-target">
                        <option value="all">全部进程 (cluster:gm:all)</option>
                        <option value="type">按进程类型 (cluster:gm:type:{t})</option>
                        <option value="pid">按进程ID (cluster:gm:pid:{p})</option>
                    </select>
                    <input class="form-control" type="number" name="type" placeholder="进程类型 100/101/102/201" style="width:180px">
                    <input class="form-control" type="number" name="pid" placeholder="进程ID" style="width:120px">
                </label>
                <br><br>
                <label>命令：
                    <input class="form-control" type="text" name="cmd" size="72" placeholder="如: info / processes / actors / spawn 201 / actor 203 1 status" required>
                </label>
                <button class="btn btn-primary" type="submit">发送</button>
            </form>
            <div class="dim">
                常用：<code>help</code> <code>info</code> <code>processes</code> <code>actors</code>
                <code>spawn 201 game-gm</code> <code>actor 203 &lt;battleId&gt; status</code> <code>shutdown</code>
            </div>
        </section>

        <section class="node-group">
            <h2>应答日志 <span class="dim">（cluster:gm:replies，最新在前）</span></h2>
            <?php if ($r === null): ?>
                <div class="empty">⚠️ 无法连接 Redis。</div>
            <?php elseif (empty($replies)): ?>
                <div class="empty">暂无应答 —— 发送一条命令后，各进程会把结果写回这里。</div>
            <?php else: ?>
                <table class="table table-vcenter">
                    <thead><tr><th>#</th><th>应答</th></tr></thead>
                    <tbody>
                    <?php foreach ($replies as $i => $line): ?>
                        <tr>
                            <td class="mono"><?= count($replies) - $i ?></td>
                            <td class="mono"><?= esc($line) ?></td>
                        </tr>
                    <?php endforeach; ?>
                    </tbody>
                </table>
            <?php endif; ?>
        </section>
<?php layout_footer(); ?>
