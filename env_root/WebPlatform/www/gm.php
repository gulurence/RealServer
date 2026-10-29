<?php
// GM 命令控制台 — 向服务器框架层与 Actor 发送命令 (K8s 风格 kubectl exec)
//
// 命令方向: PUBLISH 到 cluster:gm:all / cluster:gm:type:{t} / cluster:gm:pid:{p}
// 应答方向: 读 Redis 列表 cluster:gm:replies (CGmCommandMgr::LogReply RPUSH 封顶 1000)
require_once __DIR__ . '/_layout.php';

const GM_REPLY_KEY = 'cluster:gm:replies';
const GM_REPLIES_PER_PAGE = 10;   // 应答日志每页条数

/** 应答日志分页/查询链接: 保留 q/page, 覆盖指定参数 (null/'' 移除) */
function gm_href(array $overrides): string
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
    return 'gm.php' . ($qs !== '' ? '?' . $qs : '');
}

function gm_page_href(int $page): string
{
    return gm_href(['page' => $page]);
}

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
$total = count($replies);

// 应答内容查询 (子串, 不区分大小写)
$q = trim((string)($_GET['q'] ?? ''));
if ($q !== '') {
    $replies = array_values(array_filter($replies, fn($line) => stripos((string)$line, $q) !== false));
}
$filteredTotal = count($replies);

// 分页: 每页 GM_REPLIES_PER_PAGE 条
$pages = max(1, (int)ceil($filteredTotal / GM_REPLIES_PER_PAGE));
$page  = max(1, min((int)($_GET['page'] ?? 1), $pages));
$offset = ($page - 1) * GM_REPLIES_PER_PAGE;
$pageReplies = array_slice($replies, $offset, GM_REPLIES_PER_PAGE);
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
            <?php else: ?>
                <form method="get" action="gm.php" class="form-row" style="margin-bottom:14px;">
                    <label style="flex:1 1 280px;">
                        <span>查询（应答内容关键字）</span>
                        <input type="text" name="q" value="<?= esc($q) ?>" class="form-control" placeholder="输入应答内容关键字">
                    </label>
                    <button type="submit" class="btn btn-primary">查询</button>
                    <?php if ($q !== ''): ?><a class="btn btn-link" href="<?= esc(gm_href(['q' => null, 'page' => null])) ?>">清除</a><?php endif; ?>
                </form>

                <?php if ($total === 0): ?>
                    <div class="empty">暂无应答 —— 发送一条命令后，各进程会把结果写回这里。</div>
                <?php elseif ($filteredTotal === 0): ?>
                    <div class="empty">没有匹配「<?= esc($q) ?>」的应答。</div>
                <?php else: ?>
                    <table class="table table-vcenter">
                        <thead><tr><th>#</th><th>应答</th></tr></thead>
                        <tbody>
                        <?php foreach ($pageReplies as $j => $line): ?>
                            <tr>
                                <td class="mono"><?= $offset + $j + 1 ?></td>
                                <td class="mono"><?= esc($line) ?></td>
                            </tr>
                        <?php endforeach; ?>
                        </tbody>
                    </table>

                    <?php if ($pages > 1): ?>
                    <div class="actions" style="justify-content:space-between; align-items:center; flex-wrap:wrap;">
                        <div class="dim">第 <?= $page ?> / <?= $pages ?> 页 · 共 <?= $filteredTotal ?> 条</div>
                        <div class="actions">
                            <?php if ($page > 1): ?>
                                <a class="btn btn-sm btn-outline-secondary" href="<?= esc(gm_page_href($page - 1)) ?>">上一页</a>
                            <?php endif; ?>
                            <?php if ($page < $pages): ?>
                                <a class="btn btn-sm btn-outline-secondary" href="<?= esc(gm_page_href($page + 1)) ?>">下一页</a>
                            <?php endif; ?>
                        </div>
                    </div>
                    <?php endif; ?>
                <?php endif; ?>
            <?php endif; ?>
        </section>
<?php layout_footer(); ?>
