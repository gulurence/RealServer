<?php
// =============================================================================
// api/account.php — 创建游戏账户 (TestClient 步骤 1)
//
// POST (JSON): { "username": "...", "password": "..." }
// 返回 (JSON):
//   200 { "ok": true, "account_id": 1, "username": "..." }
//   409 { "ok": false, "error": "username_exists" }
//   500 { "ok": false, "error": "db_unavailable" }
//
// 写入:
//   MySQL account 表 (持久化兜底) + Redis account:{id} / account:{id}:state (热数据)
// =============================================================================
require_once __DIR__ . '/../config.php';

header('Content-Type: application/json; charset=utf-8');

function json_out(array $data, int $code = 200): void
{
    http_response_code($code);
    echo json_encode($data, JSON_UNESCAPED_UNICODE);
    exit;
}

$body = json_decode(file_get_contents('php://input'), true);
$username = trim((string)($body['username'] ?? ''));
$password = (string)($body['password'] ?? '');

$res = register_account($username, $password);
if (!$res['ok']) {
    $code = $res['error'] === 'missing_fields' ? 400 : 500;
    json_out(['ok' => false, 'error' => $res['error']], $code);
}
json_out(['ok' => true, 'account_id' => $res['account_id'], 'username' => $username]);
