<?php
// Actor 实例视图 — 读 Redis cluster:actor:* 注册数据 (K8s 风格框架管理)
// 由 CActorMgr::Install/UnInstall 经 CGmCommandMgr::RegisterActor/DeregisterActor 写入:
//   cluster:actor:{type}:{id}    → Hash {type,id,name,pid,ptype,state,created}
//   cluster:actor:by_type:{type} → Set  {id...}
require_once __DIR__ . '/_layout.php';

/** 每类型每页显示条数 */
const ACTORS_PER_PAGE = 10;

/** 下线列表每页显示条数 */
const OFFLINE_PER_PAGE = 10;

/**
 * 拉取 Actor 实例, 每类型按业务 ID 升序分页 (每页 ACTORS_PER_PAGE 条)。
 *
 * @return array<int, array{actors:list<array<string,string>>, total:int, page:int, pages:int}>
 *         type → {actors: 本页实例, total: 该类型总数, page: 当前页, pages: 总页数}
 */
function fetch_actors(?RedisCluster $r): array
{
    $result = [];
    if ($r === null) return $result;
    foreach (array_keys(ACTOR_TYPES) as $type) {
        $members = $r->smembers("cluster:actor:by_type:{$type}");
        if (!is_array($members) || $members === []) continue;
        sort($members, SORT_NUMERIC);                       // 稳定排序, 保证分页连续
        $total = count($members);
        $pages = max(1, (int)ceil($total / ACTORS_PER_PAGE));
        $page  = (int)($_GET["p{$type}"] ?? 1);
        $page  = max(1, min($page, $pages));

        $actors = [];
        foreach (array_slice($members, ($page - 1) * ACTORS_PER_PAGE, ACTORS_PER_PAGE) as $id) {
            $hash = $r->hgetall("cluster:actor:{$type}:{$id}");
            if (!is_array($hash) || empty($hash)) continue;
            $hash['type'] = (string)$type;
            $actors[] = $hash;
        }
        $result[$type] = [
            'actors' => $actors,
            'total'  => $total,
            'page'   => $page,
            'pages'  => $pages,
        ];
    }
    return $result;
}

/** 分页链接: 保留所有页码参数 (actor 的 p{type} 与下线列表的 offline_page), 覆盖指定参数, 剔除 partial */
function page_href(array $overrides): string
{
    $params = [];
    foreach ($_GET as $k => $v) {
        if (preg_match('/^p\d+$/', (string)$k) || $k === 'offline_page') {
            $params[(string)$k] = (string)$v;
        }
    }
    foreach ($overrides as $k => $v) {
        $params[(string)$k] = (string)$v;
    }
    return 'actors.php?' . http_build_query($params);
}

function actors_page_href(int $type, int $page): string
{
    return page_href(["p{$type}" => $page]);
}

/** Unix 秒 → 'Y-m-d H:i:s' (无/非法时为 '—') */
function fmt_created(?string $ts): string
{
    $t = (int)$ts;
    return $t > 0 ? date('Y-m-d H:i:s', $t) : '—';
}

/** Unix 秒 → 存活时长 (无/非法时为 '—') */
function fmt_alive(?string $ts): string
{
    $t = (int)$ts;
    if ($t <= 0) return '—';
    $sec = time() - $t;
    if ($sec < 0) return '—';
    $d = intdiv($sec, 86400);
    $h = intdiv($sec % 86400, 3600);
    $m = intdiv($sec % 3600, 60);
    $s = $sec % 60;
    if ($d > 0) return "{$d}天{$h}时{$m}分";
    if ($h > 0) return "{$h}时{$m}分{$s}秒";
    if ($m > 0) return "{$m}分{$s}秒";
    return "{$s}秒";
}

$r = redis();
$actorsByType = fetch_actors($r);
$total = array_sum(array_map(fn($i) => $i['total'], $actorsByType));

// 下线列表 (offline:players, 独立分页)
$offlineAll = $r !== null ? $r->smembers('offline:players') : [];
if (!is_array($offlineAll)) $offlineAll = [];
sort($offlineAll, SORT_NUMERIC);                        // 稳定排序, 保证分页连续
$offlineTotal = count($offlineAll);
$offlinePages = max(1, (int)ceil($offlineTotal / OFFLINE_PER_PAGE));
$offlinePage  = (int)($_GET['offline_page'] ?? 1);
$offlinePage  = max(1, min($offlinePage, $offlinePages));
$offline = array_slice($offlineAll, ($offlinePage - 1) * OFFLINE_PER_PAGE, OFFLINE_PER_PAGE);
?>
<?php layout_header('Actor 实例', 'actors'); ?>
<?php autorefresh(); ?>
<?php live_region_start(); ?>
        <div class="summary">
            Actor 实例总数：<b><?= $total ?></b>
            <span class="dim">（每 5 秒自动刷新）</span>
        </div>

        <?php if ($r === null): ?>
            <div class="empty">⚠️ 无法连接 Redis，请确认 Redis 已启动且 RealServer 已注册 Actor。</div>
        <?php elseif ($total === 0): ?>
            <div class="empty">暂无 Actor 实例 —— 启动 RealServer 后，Actor 安装/卸载会写入 <code>cluster:actor:*</code>。</div>
        <?php else: ?>
            <?php foreach ($actorsByType as $type => $info): ?>
                <section class="node-group">
                    <h2><?= esc(ACTOR_TYPES[$type] ?? $type) ?> <span class="dim">(type=<?= $type ?> · 共 <?= $info['total'] ?> 个)</span></h2>
                    <table class="table table-vcenter">
                        <thead>
                            <tr><th>Actor ID</th><th>业务ID</th><th>名称</th><th>承载进程</th><th>进程ID</th><th>状态</th><th>创建时间</th><th>存活时间</th><th>操作</th></tr>
                        </thead>
                        <tbody>
                        <?php foreach ($info['actors'] as $a): ?>
                            <tr>
                                <td class="mono"><?= esc($a['gid'] ?? $a['id'] ?? '') ?></td>
                                <td class="mono"><?= esc($a['id'] ?? '') ?></td>
                                <td><?= esc($a['name'] ?? '') ?></td>
                                <td><?= esc(PROCESS_TYPES[(int)($a['ptype'] ?? 0)] ?? $a['ptype']) ?></td>
                                <td class="mono"><?= esc($a['pid'] ?? '') ?></td>
                                <td><span class="badge"><?= ($a['state'] ?? '') === '1' ? 'active' : esc($a['state'] ?? '') ?></span></td>
                                <td class="mono"><?= esc(fmt_created($a['created'] ?? null)) ?></td>
                                <td><?= esc(fmt_alive($a['created'] ?? null)) ?></td>
                                <td><a class="btn btn-sm" href="actor_info.php?type=<?= (int)($a['type'] ?? 0) ?>&id=<?= (int)($a['id'] ?? 0) ?>">详细信息</a></td>
                            </tr>
                        <?php endforeach; ?>
                        </tbody>
                    </table>
                    <?php if ($info['pages'] > 1): ?>
                    <div class="actions" style="justify-content:space-between; align-items:center; flex-wrap:wrap;">
                        <div class="dim">第 <?= $info['page'] ?> / <?= $info['pages'] ?> 页 · 共 <?= $info['total'] ?> 个</div>
                        <div class="actions">
                            <?php if ($info['page'] > 1): ?>
                                <a class="btn btn-sm btn-outline-secondary" href="<?= esc(actors_page_href((int)$type, $info['page'] - 1)) ?>">上一页</a>
                            <?php endif; ?>
                            <?php if ($info['page'] < $info['pages']): ?>
                                <a class="btn btn-sm btn-outline-secondary" href="<?= esc(actors_page_href((int)$type, $info['page'] + 1)) ?>">下一页</a>
                            <?php endif; ?>
                        </div>
                    </div>
                    <?php endif; ?>
                </section>
            <?php endforeach; ?>
        <?php endif; ?>

        <?php if ($r !== null): ?>
            <section class="node-group">
                <h2>下线列表 <span class="dim">(offline:players · 共 <?= $offlineTotal ?> 个)</span></h2>
                <?php if ($offlineTotal === 0): ?>
                    <div class="empty">暂无离线玩家</div>
                <?php else: ?>
                    <table class="table table-vcenter">
                        <thead>
                            <tr><th>Role ID</th><th>昵称</th><th>等级</th><th>下线时间</th><th>离线时长</th></tr>
                        </thead>
                        <tbody>
                        <?php foreach ($offline as $pid):
                            $pidStr = (string)$pid;
                            $attrs  = explode('|', (string)$r->get("player:{$pidStr}"));
                            $name   = $attrs[0] ?? '';
                            $level  = $attrs[1] ?? '';
                            $ts     = (int)$r->get("offline:ts:{$pidStr}");
                        ?>
                            <tr>
                                <td class="mono"><?= esc($pidStr) ?></td>
                                <td><?= esc($name) ?></td>
                                <td><?= esc($level) ?></td>
                                <td class="mono"><?= $ts > 0 ? date('Y-m-d H:i:s', $ts) : '—' ?></td>
                                <td><?= $ts > 0 ? esc(fmt_alive((string)$ts)) : '—' ?></td>
                            </tr>
                        <?php endforeach; ?>
                        </tbody>
                    </table>
                    <?php if ($offlinePages > 1): ?>
                    <div class="actions" style="justify-content:space-between; align-items:center; flex-wrap:wrap;">
                        <div class="dim">第 <?= $offlinePage ?> / <?= $offlinePages ?> 页 · 共 <?= $offlineTotal ?> 个</div>
                        <div class="actions">
                            <?php if ($offlinePage > 1): ?>
                                <a class="btn btn-sm btn-outline-secondary" href="<?= esc(page_href(['offline_page' => $offlinePage - 1])) ?>">上一页</a>
                            <?php endif; ?>
                            <?php if ($offlinePage < $offlinePages): ?>
                                <a class="btn btn-sm btn-outline-secondary" href="<?= esc(page_href(['offline_page' => $offlinePage + 1])) ?>">下一页</a>
                            <?php endif; ?>
                        </div>
                    </div>
                    <?php endif; ?>
                <?php endif; ?>
            </section>
        <?php endif; ?>
<?php live_region_end(); ?>
<?php layout_footer(); ?>
