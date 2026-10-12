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
?>
<?php layout_header('在线账户', 'accounts'); ?>
<?php autorefresh(); ?>
<?php live_region_start(); ?>
        <div class="summary">
            在线账户：<b><?= $onlineCount ?></b>
            · 账户总数：<b><?= $totalAccounts ?></b>
            · 角色：<b><?= $totalRoles ?></b>
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
                    <table class="table table-vcenter">
                        <thead>
                            <tr>
                                <th>账户ID</th><th>用户名</th><th>登录状态</th><th>在线</th><th>活跃会话</th>
                            </tr>
                        </thead>
                        <tbody>
                        <?php foreach ($accountIds as $id): ?>
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
                </section>
            <?php endif; ?>

            <section class="node-group">
                <h2>角色信息 <span class="dim">(player:{id})</span></h2>
                <?php if ($totalRoles === 0): ?>
                    <div class="empty">暂无角色数据 —— 选角后写入 Redis <code>player:{id}</code>。</div>
                <?php else: ?>
                    <table class="table table-vcenter">
                        <thead>
                            <tr>
                                <th>角色ID</th><th>角色名</th><th>等级</th><th>职业</th><th>所属账户</th><th>状态</th>
                            </tr>
                        </thead>
                        <tbody>
                        <?php foreach ($roles as $roleId => $role): ?>
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
                <?php endif; ?>
            </section>
        <?php endif; ?>
<?php live_region_end(); ?>
<?php layout_footer(); ?>
