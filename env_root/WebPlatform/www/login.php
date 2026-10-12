<?php
// 登录校验 — 优先 MySQL users 表, 失败则用 config.php 兜底管理员
require_once __DIR__ . '/config.php';

session_start();
$user = trim($_POST['username'] ?? '');
$pass = (string)($_POST['password'] ?? '');

$authed = false;

// 1) MySQL 账号
$pdo = db();
if ($pdo && $user !== '') {
    $stmt = $pdo->prepare('SELECT username, password, role FROM users WHERE username = ?');
    $stmt->execute([$user]);
    $row = $stmt->fetch(PDO::FETCH_ASSOC);
    if ($row && password_verify($pass, $row['password'])) {
        $_SESSION['user'] = $row['username'];
        $_SESSION['role'] = $row['role'];
        $authed = true;
    }
}

// 2) 兜底管理员 (MySQL 不可用或未命中时)
if (!$authed && $user === FALLBACK_ADMIN_USER && $pass === FALLBACK_ADMIN_PASS) {
    $_SESSION['user'] = $user;
    $_SESSION['role'] = 'admin';
    $authed = true;
}

if ($authed) {
    header('Location: nodes.php');
} else {
    header('Location: index.php?err=1');
}
exit;
