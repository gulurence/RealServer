<?php
// =============================================================================
// 数据库账户/角色查询 — 直接查 MySQL account / player 表 (持久化兜底数据)
//
// 与 accounts.php (Redis 在线热数据) 互补:
//   - accounts.php  读 Redis account:{id}:state / player:{id} —— 只反映「在线/热」状态
//   - db_accounts.php 读 MySQL account / player —— 反映「持久化」的完整账户与角色
//
// 不依赖 Redis, 走 db() (PDO) 直连 MySQL。
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

function account_state_label(int $state): string
{
    return ACCOUNT_STATES[$state] ?? "Unknown({$state})";
}

/**
 * 构造账户查找条件 (账户ID 精确 / 用户名子串忽略大小写)
 * @return array{0:string, 1:array} [WHERE 子句, 绑定参数]
 */
function account_where(string $q): array
{
    if ($q === '') return ['', []];
    if (ctype_digit($q)) return ['WHERE id = ? OR username LIKE ?', [(int)$q, "%{$q}%"]];
    return ['WHERE username LIKE ?', ["%{$q}%"]];
}

/**
 * 全量统计 (未过滤, 供 summary 展示)
 * @return array{accounts:int, roles:int}
 */
function count_totals(?PDO $pdo): array
{
    if ($pdo === null) return ['accounts' => 0, 'roles' => 0];
    $acc  = (int)$pdo->query('SELECT COUNT(*) FROM account')->fetchColumn();
    $role = (int)$pdo->query('SELECT COUNT(*) FROM player')->fetchColumn();
    return ['accounts' => $acc, 'roles' => $role];
}

/**
 * 匹配账户总数 (SQL COUNT, 用于分页)
 */
function count_accounts(?PDO $pdo, string $q): int
{
    if ($pdo === null) return 0;
    [$where, $params] = account_where($q);
    $stmt = $pdo->prepare("SELECT COUNT(*) FROM account {$where}");
    $stmt->execute($params);
    return (int)$stmt->fetchColumn();
}

/**
 * SQL 分页拉取账户 + 本页账户的角色 (IN 查询, 不全表载入)
 * @return list<array{id:int, username:string, state:int, created_at:string, roles:list<array>}>
 */
function fetch_accounts_page(?PDO $pdo, string $q, int $per, int $offset): array
{
    if ($pdo === null) return [];

    [$where, $params] = account_where($q);

    // 本页账户 (per/offset 已在上层校验为整数, 直接内联)
    $pageStmt = $pdo->prepare("SELECT id, username, state, created_at FROM account {$where} ORDER BY id ASC LIMIT {$per} OFFSET {$offset}");
    $pageStmt->execute($params);
    $accounts = [];
    $ids = [];
    while ($row = $pageStmt->fetch(PDO::FETCH_ASSOC)) {
        $id = (int)$row['id'];
        $ids[] = $id;
        $accounts[$id] = [
            'id'         => $id,
            'username'   => (string)$row['username'],
            'state'      => (int)$row['state'],
            'created_at' => (string)$row['created_at'],
            'roles'      => [],
        ];
    }

    // 仅查本页账户的角色 (IN 查询, 避免全表)
    if ($ids !== []) {
        $in = implode(',', array_fill(0, count($ids), '?'));
        $roleStmt = $pdo->prepare("SELECT id, account_id, role_name, level, job, state, created_at FROM player WHERE account_id IN ({$in}) ORDER BY id ASC");
        $roleStmt->execute($ids);
        while ($row = $roleStmt->fetch(PDO::FETCH_ASSOC)) {
            $aid = (int)$row['account_id'];
            if (isset($accounts[$aid])) {
                $accounts[$aid]['roles'][] = [
                    'id'         => (int)$row['id'],
                    'role_name'  => (string)$row['role_name'],
                    'level'      => (int)$row['level'],
                    'job'        => (int)$row['job'],
                    'state'      => (int)$row['state'],
                    'created_at' => (string)$row['created_at'],
                ];
            }
        }
    }

    return array_values($accounts);
}

$pdo = db();
$totals = count_totals($pdo);          // 全量账户/角色数 (summary 展示)
$dbTotal = $totals['accounts'];
$dbRoles = $totals['roles'];

// ---- 查找: 账户ID / 用户名 (单个关键字同时匹配两者) ----
$q = trim((string)($_GET['q'] ?? ''));

// ---- 分页: 可选每页行数 (SQL LIMIT/OFFSET) ----
$per = (int)($_GET['per'] ?? 8);
if (!in_array($per, [8, 10, 20, 50, 100], true)) $per = 8;
$filteredTotal = count_accounts($pdo, $q);                  // SQL COUNT
$totalPages = max(1, (int)ceil($filteredTotal / $per));
$page = (int)($_GET['page'] ?? 1);
$page = max(1, min($page, $totalPages));
$offset = ($page - 1) * $per;
$pageAccounts = fetch_accounts_page($pdo, $q, $per, $offset);   // SQL LIMIT/OFFSET

$totalRoles = array_sum(array_map(fn($a) => count($a['roles']), $pageAccounts));
?>
<?php layout_header('数据库账户', 'db_accounts'); ?>
<?php autorefresh(); ?>
<?php live_region_start(); ?>
        <div class="summary">
            账户总数：<b><?= $dbTotal ?></b>
            · 角色总数：<b><?= $dbRoles ?></b>
            <?php if ($q !== ''): ?> · 匹配账户：<b><?= $filteredTotal ?></b><?php endif; ?>
            <span class="dim">（数据源：MySQL account / player 持久化表，每 10 秒自动刷新）</span>
        </div>

        <?php if ($pdo === null): ?>
            <div class="empty">⚠️ 无法连接 MySQL，请确认数据库已启动且 <code>config.php</code> 配置正确。</div>
        <?php elseif ($dbTotal === 0): ?>
            <div class="empty">暂无账户 —— 通过 <code>api/account.php</code> 创建账户后，这里会显示持久化的账户与角色。</div>
        <?php else: ?>
            <section class="node-group">
                <h2>账户列表 <span class="dim">(account 表)</span></h2>

                <form method="get" action="db_accounts.php" class="form-row" style="margin-bottom:14px;">
                    <label style="flex:1 1 240px;">
                        <span>查找（账户ID 或 用户名）</span>
                        <input type="text" name="q" value="<?= esc($q) ?>" class="form-control" placeholder="输入账户ID 或 用户名关键字">
                    </label>
                    <label>
                        <span>每页行数</span>
                        <select name="per" class="form-select">
                            <?php foreach ([8, 10, 20, 50, 100] as $opt): ?>
                                <option value="<?= $opt ?>" <?= $per === $opt ? 'selected' : '' ?>><?= $opt ?> 行/页</option>
                            <?php endforeach; ?>
                        </select>
                    </label>
                    <button type="submit" class="btn btn-primary">查找</button>
                    <?php if ($q !== ''): ?><a class="btn btn-link" href="db_accounts.php?per=<?= $per ?>">清除</a><?php endif; ?>
                </form>

                <?php if ($filteredTotal === 0): ?>
                    <div class="empty">没有匹配「<?= esc($q) ?>」的账户。</div>
                <?php else: ?>
                    <table class="table table-vcenter">
                        <thead>
                            <tr>
                                <th>账户ID</th><th>用户名</th><th>状态</th><th>角色数</th><th>创建时间</th>
                            </tr>
                        </thead>
                        <tbody>
                        <?php foreach ($pageAccounts as $acc): ?>
                            <tr>
                                <td class="mono"><?= $acc['id'] ?></td>
                                <td><?= esc($acc['username']) ?></td>
                                <td><span class="badge"><?= esc(account_state_label($acc['state'])) ?></span></td>
                                <td><?= count($acc['roles']) ?></td>
                                <td class="mono"><?= esc($acc['created_at']) ?></td>
                            </tr>
                        <?php endforeach; ?>
                        </tbody>
                    </table>

                    <div class="actions" style="justify-content:space-between; align-items:center; flex-wrap:wrap;">
                        <div class="dim">第 <?= $page ?> / <?= $totalPages ?> 页 · 共 <?= $filteredTotal ?> 个账户</div>
                        <div class="actions">
                            <?php if ($page > 1): ?>
                                <a class="btn btn-sm btn-outline-secondary" href="db_accounts.php?q=<?= urlencode($q) ?>&per=<?= $per ?>&page=<?= $page - 1 ?>">上一页</a>
                            <?php endif; ?>
                            <?php if ($page < $totalPages): ?>
                                <a class="btn btn-sm btn-outline-secondary" href="db_accounts.php?q=<?= urlencode($q) ?>&per=<?= $per ?>&page=<?= $page + 1 ?>">下一页</a>
                            <?php endif; ?>
                        </div>
                    </div>
                <?php endif; ?>
            </section>

            <section class="node-group">
                <h2>角色列表 <span class="dim">(player 表)</span></h2>
                <?php if ($totalRoles === 0): ?>
                    <div class="empty">本页账户暂无角色 —— 选角后写入 MySQL <code>player</code> 表。</div>
                <?php else: ?>
                    <table class="table table-vcenter">
                        <thead>
                            <tr>
                                <th>角色ID</th><th>角色名</th><th>等级</th><th>职业</th><th>所属账户</th><th>状态</th><th>创建时间</th>
                            </tr>
                        </thead>
                        <tbody>
                        <?php foreach ($pageAccounts as $acc): ?>
                            <?php foreach ($acc['roles'] as $role): ?>
                            <tr>
                                <td class="mono"><?= $role['id'] ?></td>
                                <td><?= esc($role['role_name']) ?></td>
                                <td><?= $role['level'] ?></td>
                                <td><?= $role['job'] ?></td>
                                <td class="mono"><?= $acc['id'] ?> (<?= esc($acc['username']) ?>)</td>
                                <td><?= $role['state'] ?></td>
                                <td class="mono"><?= esc($role['created_at']) ?></td>
                            </tr>
                            <?php endforeach; ?>
                        <?php endforeach; ?>
                        </tbody>
                    </table>
                <?php endif; ?>
            </section>
        <?php endif; ?>
<?php live_region_end(); ?>
<?php layout_footer(); ?>
