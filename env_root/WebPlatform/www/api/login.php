<?php
// =============================================================================
// api/login.php — 游戏账户登录, 签发 Session 令牌 (TestClient 步骤 2)
//
// POST (JSON): { "username": "...", "password": "..." }
// 返回 (JSON):
//   200 { "ok": true, "token": "…", "account_id": 1 }
//   401 { "ok": false, "error": "invalid_credentials" }
//   500 { "ok": false, "error": "db_unavailable" }
//
// 写入:
//   Redis session:{token} → account_id (TTL)
//   Redis account:{id}:state → 1 (LoggedIn)
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

if ($username === '' || $password === '') {
    json_out(['ok' => false, 'error' => 'missing_fields'], 400);
}

$pdo = db();
if ($pdo === null) {
    json_out(['ok' => false, 'error' => 'db_unavailable'], 500);
}

$stmt = $pdo->prepare('SELECT id, username, password FROM account WHERE username = ?');
$stmt->execute([$username]);
$row = $stmt->fetch(PDO::FETCH_ASSOC);

if (!$row || !password_verify($password, $row['password'])) {
    json_out(['ok' => false, 'error' => 'invalid_credentials'], 401);
}

$accountId = (int)$row['id'];

// 签发 Session 令牌, 写 Redis
$token = make_token();
$r = redis();
if ($r !== null) {
    $r->setex(session_key($token), SESSION_TTL, (string)$accountId);
    $r->set(game_account_state_key($accountId), '1');   // 1 = LoggedIn
}

json_out(['ok' => true, 'token' => $token, 'account_id' => $accountId, 'username' => $row['username']]);
