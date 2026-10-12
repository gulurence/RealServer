<?php
// 登录页 — 已登录直接跳节点状态页
session_start();
if (!empty($_SESSION['user'])) {
    header('Location: nodes.php');
    exit;
}
$err = isset($_GET['err']);
?>
<!DOCTYPE html>
<html lang="zh-CN" data-bs-theme="dark">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>RealServer 控制台 — 登录</title>
    <link rel="stylesheet" href="assets/tabler.min.css">
    <link rel="stylesheet" href="style.css">
</head>
<body class="d-flex flex-column">
    <div class="page page-center">
        <div class="container container-tight py-4">
            <div class="text-center mb-4">
                <span class="brand-dot"></span>
                <span class="navbar-brand navbar-brand-autodark fs-3 fw-bold">RealServer 控制台</span>
            </div>
            <div class="card card-md">
                <div class="card-body">
                    <h2 class="h2 text-center mb-4">登录</h2>
                    <?php if ($err): ?>
                        <div class="alert alert-danger" role="alert">用户名或密码错误</div>
                    <?php endif; ?>
                    <form method="post" action="login.php" autocomplete="off">
                        <div class="mb-3">
                            <label class="form-label">用户名</label>
                            <input type="text" name="username" class="form-control"
                                   autocomplete="username" required autofocus>
                        </div>
                        <div class="mb-3">
                            <label class="form-label">密码</label>
                            <input type="password" name="password" class="form-control"
                                   autocomplete="current-password" required>
                        </div>
                        <div class="form-footer">
                            <button type="submit" class="btn btn-primary w-100">登录</button>
                        </div>
                    </form>
                </div>
                <div class="text-center text-secondary mb-4" style="font-size:13px">
                    测试账号 admin / admin
                </div>
            </div>
        </div>
    </div>
</body>
</html>
