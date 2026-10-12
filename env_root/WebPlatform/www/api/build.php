<?php
// =============================================================================
// build.php — Jenkins 构建完成后的登记端点（裸 API，不走 session/layout）
//
// Jenkins 编译成功后 POST 到这里：token + version(=ts.md5 完整串) + artifact。
// 本端点只做两件事，不做滚动：
//   1. 把 exe 拷成版本化副本 releases/RealServer_<version>.exe（绕开「运行中单 exe 文件锁」）
//   2. LPUSH cluster:build:versions <version>（构建版本索引，灰度面板据此列版本）
// 滚动改由面板「更新」按钮 → api/gray_update.php POST 人工触发。
// =============================================================================
require_once __DIR__ . '/../config.php';

set_time_limit(0);
header('Content-Type: application/json; charset=utf-8');

function build_out(array $a, int $code = 200): void
{
    http_response_code($code);
    echo json_encode($a, JSON_UNESCAPED_UNICODE);
    exit;
}

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    build_out(['ok' => false, 'error' => 'method_not_allowed'], 405);
}

$token    = trim((string)($_POST['token'] ?? ''));
$version  = trim((string)($_POST['version'] ?? ''));
$artifact = trim((string)($_POST['artifact'] ?? ''));

if ($token !== DEPLOY_TOKEN) {
    build_out(['ok' => false, 'error' => 'bad_token'], 403);
}
if ($version === '') {
    build_out(['ok' => false, 'error' => 'missing_version'], 400);
}
if ($artifact === '' || !is_file($artifact)) {
    build_out(['ok' => false, 'error' => 'bad_artifact'], 400);
}

const BUILD_VERSIONS_KEY = 'cluster:build:versions';

$r = redis();
if ($r === null) {
    build_out(['ok' => false, 'error' => 'redis_unavailable'], 500);
}

$binDir       = 'E:\RealServer\bin\windows';
$releasesDir  = $binDir . '\releases';
$versionedExe = $releasesDir . '\RealServer_' . $version . '.exe';

// 1. 版本化 exe 副本（不覆盖运行中的 bin\windows\RealServer.exe，避免文件锁）
if (!is_dir($releasesDir)) {
    mkdir($releasesDir, 0755, true);
}
if (!copy($artifact, $versionedExe)) {
    build_out(['ok' => false, 'error' => 'copy_artifact_failed'], 500);
}

// 2. 登记构建版本索引 (最新在前；灰度面板 §3 用 LRANGE 读取)
$r->lpush(BUILD_VERSIONS_KEY, $version);

build_out(['ok' => true, 'version' => $version, 'artifact' => $versionedExe]);
