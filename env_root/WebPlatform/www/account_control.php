<?php
// =============================================================================
// 账户控制页 — 账号注册 + 账户列表
//
// 注册：表单提交用户名/密码 → register_account()（config.php）写入 MySQL account
//      表 (bcrypt) + Redis account:{id} / account:{id}:state 热数据，与 api/account.php、
//      api/login.php、script/common/account.lua 键约定一致。同名账户幂等返回已有 id。
// 列表：直查 MySQL account 表（持久化兜底），与 db_accounts.php 同源。
// =============================================================================
require_once __DIR__ . '/_layout.php';

// 账户状态机标签（镜像 msg_id.proto AccountState 枚举）
const ACCOUNT_STATES = [
    0 => '已创建',
    1 => '已登录',
    2 => '已拉取角色',
    3 => '已选角',
    4 => '游戏中',
];

const ACCOUNTS_PER_PAGE = 10;   // 账户列表每页条数

function acc_state_label(int $state): string
{
    return ACCOUNT_STATES[$state] ?? "Unknown({$state})";
}

/** 账户列表分页/搜索链接: 保留 q/page, 覆盖指定参数 (null/'' 移除) */
function acc_href(array $overrides): string
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
    return 'account_control.php' . ($qs !== '' ? '?' . $qs : '');
}

function acc_page_href(int $page): string
{
    return acc_href(['page' => $page]);
}

// ---------- 注册处理 ----------
$notice = '';
$noticeOk = true;
if ($_SERVER['REQUEST_METHOD'] === 'POST' && post_str('action') === 'register') {
    $username = post_str('username');
    $password = (string)($_POST['password'] ?? '');
    $confirm  = (string)($_POST['confirm'] ?? '');

    if ($username === '' || $password === '') {
        $notice = '账号与密码不能为空';
        $noticeOk = false;
    } elseif (strlen($password) < 4) {
        $notice = '密码至少 4 个字符';
        $noticeOk = false;
    } elseif ($password !== $confirm) {
        $notice = '两次输入的密码不一致';
        $noticeOk = false;
    } else {
        $res = register_account($username, $password);
        if (!$res['ok']) {
            $notice = $res['error'] === 'db_unavailable'
                ? '无法连接 MySQL，注册失败'
                : '注册失败：' . esc($res['error']);
            $noticeOk = false;
        } else {
            $notice = "账户注册成功：{$username}（account_id={$res['account_id']}）";
        }
    }
}

// ---------- 账户列表（每页 ACCOUNTS_PER_PAGE 条, 支持账户ID/用户名搜索） ----------
$pdo = db();
$total = 0;
$accounts = [];
$q = trim((string)($_GET['q'] ?? ''));
$pages = 1;
$page = max(1, (int)($_GET['page'] ?? 1));
if ($pdo !== null) {
    // 搜索: 纯数字按 账户ID 精确 或 用户名子串 匹配; 否则按用户名子串匹配 (MySQL 默认 collation 不区分大小写)
    $where = '';
    $params = [];
    if ($q !== '') {
        if (ctype_digit($q)) {
            $where = 'WHERE id = ? OR username LIKE ?';
            $params = [(int)$q, '%' . $q . '%'];
        } else {
            $where = 'WHERE username LIKE ?';
            $params = ['%' . $q . '%'];
        }
    }

    $countStmt = $pdo->prepare("SELECT COUNT(*) FROM account {$where}");
    $countStmt->execute($params);
    $total = (int)$countStmt->fetchColumn();

    $pages = max(1, (int)ceil($total / ACCOUNTS_PER_PAGE));
    $page  = max(1, min($page, $pages));
    $offset = ($page - 1) * ACCOUNTS_PER_PAGE;

    $listStmt = $pdo->prepare("SELECT id, username, state, created_at FROM account {$where} ORDER BY id DESC LIMIT " . ACCOUNTS_PER_PAGE . " OFFSET {$offset}");
    $listStmt->execute($params);
    while ($row = $listStmt->fetch(PDO::FETCH_ASSOC)) {
        $accounts[] = $row;
    }
}

layout_header('账户控制', 'account_control');
?>
<?php if ($notice !== ''): ?>
    <div class="alert <?= $noticeOk ? 'alert-success' : 'alert-warning' ?>"><?= $notice ?></div>
<?php endif; ?>

<div class="row">
    <!-- 注册表单 -->
    <div class="col-md-5">
        <div class="card">
            <div class="card-header">账号注册 <span class="dim">写 MySQL account + Redis 热数据</span></div>
            <div class="card-body">
                <form method="post" action="account_control.php">
                    <input type="hidden" name="action" value="register">
                    <div class="mb-3">
                        <label class="form-label">账号（用户名）</label>
                        <input type="text" name="username" class="form-control" placeholder="输入账号" required>
                    </div>
                    <div class="mb-3">
                        <label class="form-label">密码</label>
                        <input type="password" name="password" class="form-control" placeholder="至少 4 个字符" required>
                    </div>
                    <div class="mb-3">
                        <label class="form-label">确认密码</label>
                        <input type="password" name="confirm" class="form-control" placeholder="再次输入密码" required>
                    </div>
                    <button type="submit" class="btn btn-primary">注册</button>
                    <span class="dim" style="margin-left:12px">同名账户幂等：已存在时返回其 account_id。</span>
                </form>
            </div>
        </div>
    </div>

    <!-- 账户列表 -->
    <div class="col-md-7">
        <div class="card">
            <div class="card-header">账户列表 <span class="dim">account 表 · 共 <?= $total ?> 个</span></div>
            <div class="card-body">
                <?php if ($pdo === null): ?>
                    <div class="empty">⚠️ 无法连接 MySQL，请确认数据库已启动且 <code>config.php</code> 配置正确。</div>
                <?php else: ?>
                    <form method="get" action="account_control.php" class="form-row" style="margin-bottom:14px;">
                        <label style="flex:1 1 240px;">
                            <span>账户查找（账户ID 或 用户名）</span>
                            <input type="text" name="q" value="<?= esc($q) ?>" class="form-control" placeholder="账户ID 或 用户名关键字">
                        </label>
                        <button type="submit" class="btn btn-primary">查找</button>
                        <?php if ($q !== ''): ?><a class="btn btn-link" href="<?= esc(acc_href(['q' => null, 'page' => null])) ?>">清除</a><?php endif; ?>
                    </form>

                    <?php if ($total === 0): ?>
                        <div class="empty"><?= $q !== '' ? '没有匹配「' . esc($q) . '」的账户。' : '暂无账户 —— 在左侧表单注册第一个账户。' ?></div>
                    <?php else: ?>
                        <div class="table-responsive"><table class="table table-vcenter">
                            <thead><tr>
                                <th>账户ID</th><th>用户名</th><th>状态</th><th>创建时间</th>
                            </tr></thead>
                            <tbody>
                            <?php foreach ($accounts as $acc): ?>
                                <tr>
                                    <td class="mono"><?= (int)$acc['id'] ?></td>
                                    <td><?= esc($acc['username']) ?></td>
                                    <td><span class="badge"><?= esc(acc_state_label((int)$acc['state'])) ?></span></td>
                                    <td class="mono"><?= esc($acc['created_at']) ?></td>
                                </tr>
                            <?php endforeach; ?>
                            </tbody>
                        </table></div>

                        <?php if ($pages > 1): ?>
                        <div class="actions" style="justify-content:space-between; align-items:center; flex-wrap:wrap;">
                            <div class="dim">第 <?= $page ?> / <?= $pages ?> 页 · 共 <?= $total ?> 个账户</div>
                            <div class="actions">
                                <?php if ($page > 1): ?>
                                    <a class="btn btn-sm btn-outline-secondary" href="<?= esc(acc_page_href($page - 1)) ?>">上一页</a>
                                <?php endif; ?>
                                <?php if ($page < $pages): ?>
                                    <a class="btn btn-sm btn-outline-secondary" href="<?= esc(acc_page_href($page + 1)) ?>">下一页</a>
                                <?php endif; ?>
                            </div>
                        </div>
                        <?php endif; ?>
                    <?php endif; ?>
                    <div class="dim" style="margin-top:8px">详细角色/持久化信息见 <a href="db_accounts.php">数据库账户</a>。</div>
                <?php endif; ?>
            </div>
        </div>
    </div>
</div>
<?php layout_footer(); ?>
