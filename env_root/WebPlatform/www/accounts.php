<?php
// 在线账户信息 — 直接从 Redis 拉取 (登录状态 + 角色信息)
//
// 数据键约定与 script/common/account.lua / config.php 一致:
//   account:{id}          → 账号信息 (JSON: account_id/username/state)
//   account:{id}:state    → 账户状态机 (0=已创建 1=已登录 2=已拉取角色 3=已选角 4=游戏中)
//   session:{token}       → 账号 id (Redis string, TTL 过期; 判定「在线」)
//   player:{id}           → 角色信息 (JSON: account_id/role_name/level/job/state)
// =============================================================================
require_once __DIR__ . '/_layout.php';

// 账户状态机标签 (镜像 msg_id.proto 的 AccountState 枚举)
const ACCOUNT_STATES = [
    0 => '已创建',
    1 => '已登录',
    2 => '已拉取角色',
    3 => '已选角',
    4 => '游戏中',
];

const ACCOUNTS_PER_PAGE = 10;   // 账户登录状态列表每页条数
const ROLES_PER_PAGE = 10;      // 角色信息列表每页条数

/**
 * SCAN 遍历 Redis 键 (避免 KEYS 阻塞)
 * @return list<string>
 */
function scan_keys(?RedisCluster $r, string $pattern, int $count = 200): array
{
    // 集群模式: 委托 config.php 的 scan_cluster 逐 master 节点 SCAN
    return scan_cluster($r, $pattern, $count);
}

/**
 * 拉取所有账户: 账户ID → [state, username]
 * 登录状态取 account:{id}:state (由 Lua setState 实时更新), 用户名取 account:{id} JSON。
 * @return array<int, array{state:int, username:string}>
 */
function fetch_accounts(?RedisCluster $r): array
{
    $accounts = [];
    if ($r === null) {
        return $accounts;
    }
    foreach (scan_keys($r, 'account:*:state') as $key) {
        if (!preg_match('/^account:(\d+):state$/', $key, $m)) {
            continue;
        }
        $id = (int)$m[1];
        $state = (int)$r->get("account:{$id}:state");

        $username = 'account_' . $id;
        $infoRaw = $r->get("account:{$id}");
        if ($infoRaw) {
            $info = json_decode($infoRaw, true);
            if (is_array($info) && !empty($info['username'])) {
                $username = (string)$info['username'];
            }
        }
        $accounts[$id] = ['state' => $state, 'username' => $username];
    }
    return $accounts;
}

/**
 * 拉取活跃会话: 账户ID → 会话数 (判定「在线」)
 * @return array<int, int>
 */
function fetch_sessions(?RedisCluster $r): array
{
    $sessions = [];
    if ($r === null) {
        return $sessions;
    }
    foreach (scan_keys($r, 'session:*') as $key) {
        $accountId = (int)$r->get($key);
        if ($accountId > 0) {
            $sessions[$accountId] = ($sessions[$accountId] ?? 0) + 1;
        }
    }
    return $sessions;
}

/**
 * 拉取角色信息: 角色ID → 解析后的角色数组 (含 account_id/role_name/level/job/state)
 * @return array<int, array>
 */
function fetch_roles(?RedisCluster $r): array
{
    $roles = [];
    if ($r === null) {
        return $roles;
    }
    foreach (scan_keys($r, 'player:*') as $key) {
        if (!preg_match('/^player:(\d+)$/', $key, $m)) {
            continue;
        }
        $roleId = (int)$m[1];
        $raw = $r->get($key);
        if ($raw === false || $raw === '') {
            continue;
        }
        $data = json_decode($raw, true);
        if (!is_array($data)) {
            $data = ['raw' => (string)$raw];
        }
        $data['role_id'] = $roleId;
        $roles[$roleId] = $data;
    }
    return $roles;
}

function account_state_label(int $state): string
{
    return ACCOUNT_STATES[$state] ?? "Unknown({$state})";
}

/** 构建 accounts.php 链接: 保留 q/page/rq/rpage, 覆盖指定参数 (null/'' 表示移除), 排除 partial */
function accounts_href(array $overrides): string
{
    $params = [];
    foreach (['q', 'page', 'rq', 'rpage'] as $k) {
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
    return 'accounts.php' . ($qs !== '' ? '?' . $qs : '');
}

function accounts_page_href(int $page): string
{
    return accounts_href(['page' => $page]);
}

function roles_page_href(int $page): string
{
    return accounts_href(['rpage' => $page]);
}

$r = redis();
$accounts = fetch_accounts($r);
$sessions = fetch_sessions($r);
$roles    = fetch_roles($r);

$onlineIds    = array_keys($sessions);   // 有活跃 session 的账户 = 在线
$onlineCount  = count($onlineIds);
$totalAccounts = count($accounts);
$totalRoles   = count($roles);

// 在线账户排前, 其余按账户ID升序
$accountIds = array_keys($accounts);
usort($accountIds, function ($a, $b) use ($sessions) {
    $oa = isset($sessions[$a]) ? 1 : 0;
    $ob = isset($sessions[$b]) ? 1 : 0;
    if ($oa !== $ob) {
        return $ob <=> $oa;
    }
    return $a <=> $b;
});

// 查询过滤: 账户ID (精确) 或 用户名 (子串, 大小写不敏感)
$q = trim((string)($_GET['q'] ?? ''));
if ($q !== '') {
    $accountIds = array_values(array_filter($accountIds, function ($id) use ($accounts, $q) {
        $acc = $accounts[$id];
        if (ctype_digit($q)) {
            return (int)$id === (int)$q || stripos((string)$acc['username'], $q) !== false;
        }
        return stripos((string)$acc['username'], $q) !== false;
    }));
}

// 分页: 每页 ACCOUNTS_PER_PAGE 条
$filteredTotal = count($accountIds);
$totalPages = max(1, (int)ceil($filteredTotal / ACCOUNTS_PER_PAGE));
$page = max(1, min((int)($_GET['page'] ?? 1), $totalPages));
$pageAccountIds = array_slice($accountIds, ($page - 1) * ACCOUNTS_PER_PAGE, ACCOUNTS_PER_PAGE);

// 角色查询过滤: 角色ID (精确) 或 角色名 (子串, 大小写不敏感)
$rq = trim((string)($_GET['rq'] ?? ''));
$roleIds = array_keys($roles);
sort($roleIds, SORT_NUMERIC);   // 稳定排序, 保证分页连续
if ($rq !== '') {
    $roleIds = array_values(array_filter($roleIds, function ($rid) use ($roles, $rq) {
        $role = $roles[$rid];
        if (ctype_digit($rq)) {
            return (int)$rid === (int)$rq || stripos((string)($role['role_name'] ?? ''), $rq) !== false;
        }
        return stripos((string)($role['role_name'] ?? ''), $rq) !== false;
    }));
}

// 角色分页: 每页 ROLES_PER_PAGE 条
$rolesFilteredTotal = count($roleIds);
$rolesTotalPages = max(1, (int)ceil($rolesFilteredTotal / ROLES_PER_PAGE));
$rpage = max(1, min((int)($_GET['rpage'] ?? 1), $rolesTotalPages));
$pageRoleIds = array_slice($roleIds, ($rpage - 1) * ROLES_PER_PAGE, ROLES_PER_PAGE);
?>
<?php layout_header('在线账户', 'accounts'); ?>
<?php autorefresh(); ?>
<?php if (!is_partial() && $r !== null && $totalAccounts > 0): ?>
<div class="d-flex flex-wrap gap-3" style="margin-bottom:14px;">
    <form method="get" action="accounts.php" class="form-row" style="flex:1 1 340px; margin:0;">
        <label style="flex:1 1 200px;">
            <span>账户查找（账户ID 或 用户名）</span>
            <input type="text" name="q" value="<?= esc($q) ?>" class="form-control" placeholder="账户ID 或 用户名关键字">
        </label>
        <button type="submit" class="btn btn-primary">查找</button>
        <?php if ($q !== ''): ?><a class="btn btn-link" href="<?= esc(accounts_href(['q' => null, 'page' => null])) ?>">清除</a><?php endif; ?>
    </form>
</div>
<?php endif; ?>
<?php if (!is_partial() && $r !== null): ?>
<script>
(function () {
    // 角色查找输入框位于自动刷新区域内, 重建 DOM 后恢复正在输入的内容, 避免被 5 秒刷新清空
    var savedRq = null;
    document.addEventListener('input', function (e) {
        if (e.target && e.target.id === 'roleSearch') savedRq = e.target.value;
    });
    var region = document.getElementById('live-region');
    if (region) {
        var mo = new MutationObserver(function () {
            var el = document.getElementById('roleSearch');
            if (el && savedRq !== null && el.value !== savedRq) el.value = savedRq;
        });
        mo.observe(region, { childList: true, subtree: true });
    }
})();
</script>
<?php endif; ?>
<?php live_region_start(); ?>
        <div class="summary">
            在线账户：<b><?= $onlineCount ?></b>
            · 账户总数：<b><?= $totalAccounts ?></b>
            · 角色：<b><?= $totalRoles ?></b>
            <?php if ($q !== ''): ?> · 匹配账户：<b><?= $filteredTotal ?></b><?php endif; ?>
            <?php if ($rq !== ''): ?> · 匹配角色：<b><?= $rolesFilteredTotal ?></b><?php endif; ?>
            <span class="dim">（每 5 秒自动刷新）</span>
        </div>

        <?php if ($r === null): ?>
            <div class="empty">⚠️ 无法连接 Redis，请确认 Redis 已启动。</div>
        <?php else: ?>
            <?php if ($totalAccounts === 0): ?>
                <div class="empty">暂无账户 —— 通过 <code>api/account.php</code> 创建账户或运行 TestClient 后，这里会显示在线账户。</div>
            <?php else: ?>
                <section class="node-group">
                    <h2>账户登录状态 <span class="dim">(account:{id}:state)</span></h2>
                    <?php if ($filteredTotal === 0): ?>
                        <div class="empty">没有匹配「<?= esc($q) ?>」的账户。</div>
                    <?php else: ?>
                    <table class="table table-vcenter">
                        <thead>
                            <tr>
                                <th>账户ID</th><th>用户名</th><th>登录状态</th><th>在线</th><th>活跃会话</th>
                            </tr>
                        </thead>
                        <tbody>
                        <?php foreach ($pageAccountIds as $id): ?>
                            <?php $acc = $accounts[$id]; $online = isset($sessions[$id]); ?>
                            <tr class="<?= $online ? 'online' : 'offline' ?>">
                                <td><?= $id ?></td>
                                <td><?= esc($acc['username']) ?></td>
                                <td><span class="badge"><?= esc(account_state_label($acc['state'])) ?></span></td>
                                <td><?= $online ? '在线' : '离线' ?></td>
                                <td><?= (int)($sessions[$id] ?? 0) ?></td>
                            </tr>
                        <?php endforeach; ?>
                        </tbody>
                    </table>

                    <?php if ($totalPages > 1): ?>
                    <div class="actions" style="justify-content:space-between; align-items:center; flex-wrap:wrap;">
                        <div class="dim">第 <?= $page ?> / <?= $totalPages ?> 页 · 共 <?= $filteredTotal ?> 个账户</div>
                        <div class="actions">
                            <?php if ($page > 1): ?>
                                <a class="btn btn-sm btn-outline-secondary" href="<?= esc(accounts_page_href($page - 1)) ?>">上一页</a>
                            <?php endif; ?>
                            <?php if ($page < $totalPages): ?>
                                <a class="btn btn-sm btn-outline-secondary" href="<?= esc(accounts_page_href($page + 1)) ?>">下一页</a>
                            <?php endif; ?>
                        </div>
                    </div>
                    <?php endif; ?>
                    <?php endif; ?>
                </section>
            <?php endif; ?>

            <section class="node-group">
                <h2>角色信息 <span class="dim">(player:{id})</span></h2>
                <?php if ($totalRoles === 0): ?>
                    <div class="empty">暂无角色数据 —— 选角后写入 Redis <code>player:{id}</code>。</div>
                <?php else: ?>
                    <form method="get" action="accounts.php" class="form-row" style="margin-bottom:14px;">
                        <label style="flex:1 1 240px;">
                            <span>角色查找（角色ID）</span>
                            <input type="text" id="roleSearch" name="rq" value="<?= esc($rq) ?>" class="form-control" placeholder="角色ID">
                        </label>
                        <button type="submit" class="btn btn-primary">查找</button>
                        <?php if ($rq !== ''): ?><a class="btn btn-link" href="<?= esc(accounts_href(['rq' => null, 'rpage' => null])) ?>">清除</a><?php endif; ?>
                    </form>

                    <?php if ($rolesFilteredTotal === 0): ?>
                        <div class="empty">没有匹配「<?= esc($rq) ?>」的角色。</div>
                    <?php else: ?>
                    <table class="table table-vcenter">
                        <thead>
                            <tr>
                                <th>角色ID</th><th>角色名</th><th>等级</th><th>职业</th><th>所属账户</th><th>状态</th>
                            </tr>
                        </thead>
                        <tbody>
                        <?php foreach ($pageRoleIds as $roleId): ?>
                            <?php $role = $roles[$roleId]; ?>
                            <tr>
                                <td><?= $roleId ?></td>
                                <td><?= esc((string)($role['role_name'] ?? ('player_' . $roleId))) ?></td>
                                <td><?= (int)($role['level'] ?? 0) ?></td>
                                <td><?= (int)($role['job'] ?? 0) ?></td>
                                <td><?= isset($role['account_id']) ? (int)$role['account_id'] : '—' ?></td>
                                <td><?= isset($role['state']) ? (int)$role['state'] : '—' ?></td>
                            </tr>
                        <?php endforeach; ?>
                        </tbody>
                    </table>

                    <?php if ($rolesTotalPages > 1): ?>
                    <div class="actions" style="justify-content:space-between; align-items:center; flex-wrap:wrap;">
                        <div class="dim">第 <?= $rpage ?> / <?= $rolesTotalPages ?> 页 · 共 <?= $rolesFilteredTotal ?> 个角色</div>
                        <div class="actions">
                            <?php if ($rpage > 1): ?>
                                <a class="btn btn-sm btn-outline-secondary" href="<?= esc(roles_page_href($rpage - 1)) ?>">上一页</a>
                            <?php endif; ?>
                            <?php if ($rpage < $rolesTotalPages): ?>
                                <a class="btn btn-sm btn-outline-secondary" href="<?= esc(roles_page_href($rpage + 1)) ?>">下一页</a>
                            <?php endif; ?>
                        </div>
                    </div>
                    <?php endif; ?>
                    <?php endif; ?>
                <?php endif; ?>
            </section>
        <?php endif; ?>
<?php live_region_end(); ?>
<?php layout_footer(); ?>
