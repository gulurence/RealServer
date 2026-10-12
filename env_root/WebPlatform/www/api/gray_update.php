<?php
// =============================================================================
// gray_update.php — 灰度更新面板后端（session 鉴权，供 gray_update.php 前端 fetch）
//
// GET  ?type=<100|101|102|201>[&target=<version>]
//   返回该类型可用构建版本列表 + 进程列表(含 latest/旧版本 判定) + 统计四项。
// POST {type, version, batch_size}
//   写 cluster:update:process:config(rolling) → 同步调 gray_roll.ps1 -Type <type> 分批
//   滚动重启 → 成功 status=done / 失败 status=paused，并记 process log。
// =============================================================================
require_once __DIR__ . '/../config.php';

session_start();
if (empty($_SESSION['user'])) {
    http_response_code(401);
    header('Content-Type: application/json; charset=utf-8');
    echo json_encode(['ok' => false, 'error' => 'unauthorized'], JSON_UNESCAPED_UNICODE);
    exit;
}

header('Content-Type: application/json; charset=utf-8');

function gu_out(array $a, int $code = 200): void
{
    http_response_code($code);
    echo json_encode($a, JSON_UNESCAPED_UNICODE);
    exit;
}

const BUILD_VERSIONS_KEY = 'cluster:build:versions';
const PROC_UPDATE_CFG_KEY = 'cluster:update:process:config';
const PROC_UPDATE_LOG_KEY = 'cluster:update:process:log';
const HEARTBEAT_TIMEOUT   = 15;

const GU_TYPE_NAMES = [
    100 => 'Gate',
    101 => 'Login',
    102 => 'Global',
    201 => 'Game',
];

function gu_online(array $h): bool
{
    $hb = (int)($h['heartbeat'] ?? 0);
    return $hb > 0 && (time() - $hb) <= HEARTBEAT_TIMEOUT;
}

$r = redis();
if ($r === null) {
    gu_out(['ok' => false, 'error' => 'redis_unavailable'], 500);
}

// ---------- GET: 列出构建版本 + 进程 + 统计 ----------
if ($_SERVER['REQUEST_METHOD'] === 'GET') {
    $type = (int)($_GET['type'] ?? 0);
    if (!isset(GU_TYPE_NAMES[$type])) {
        gu_out(['ok' => false, 'error' => 'bad_type'], 400);
    }

    $versions = [];
    try {
        $raw = $r->lrange(BUILD_VERSIONS_KEY, 0, 99);
        if (is_array($raw)) {
            $versions = $raw;
        }
    } catch (Throwable $e) {
    }
    $latest = (string)($versions[0] ?? '');

    // 目标版本: 显式 ?target= 优先, 否则回退最新构建
    $target = trim((string)($_GET['target'] ?? ''));
    if ($target === '') {
        $target = $latest;
    }

    $processes = [];
    try {
        $members = $r->smembers("cluster:process:by_type:{$type}");
        if (is_array($members)) {
            sort($members, SORT_NUMERIC);
            foreach ($members as $id) {
                $h = $r->hgetall("cluster:process:{$type}:{$id}");
                if (!is_array($h) || empty($h)) {
                    continue;
                }
                $ver = (string)($h['version'] ?? '');
                $processes[] = [
                    'id'              => (string)$id,
                    'name'            => (string)($h['name'] ?? ''),
                    'version'         => $ver,
                    'content_version' => (string)($h['content_version'] ?? ''),
                    'state'           => (string)($h['state'] ?? ''),
                    'heartbeat'       => (string)($h['heartbeat'] ?? ''),
                    'online'          => gu_online($h),
                    'is_latest'       => $ver !== '' && $latest !== '' && $ver === $latest,
                ];
            }
        }
    } catch (Throwable $e) {
    }

    $total = count($processes);
    $done = 0;
    foreach ($processes as $p) {
        if ($target !== '' && $p['version'] === $target) {
            $done++;
        }
    }
    $waiting = $total - $done;
    $percent = $total > 0 ? (int)round($done * 100 / $total) : 0;

    gu_out([
        'ok'        => true,
        'type'      => $type,
        'type_name' => GU_TYPE_NAMES[$type],
        'latest'    => $latest,
        'target'    => $target,
        'versions'  => $versions,
        'processes' => $processes,
        'stats'     => [
            'total'   => $total,
            'done'    => $done,
            'waiting' => $waiting,
            'percent' => $percent,
        ],
    ]);
}

// ---------- POST: 触发灰度滚动 ----------
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $type      = (int)($_POST['type'] ?? 0);
    $version   = trim((string)($_POST['version'] ?? ''));
    $batchSize = max(1, (int)($_POST['batch_size'] ?? 1));

    if (!isset(GU_TYPE_NAMES[$type])) {
        gu_out(['ok' => false, 'error' => 'bad_type'], 400);
    }
    if ($version === '') {
        gu_out(['ok' => false, 'error' => 'missing_version'], 400);
    }

    $binDir       = 'E:\RealServer\bin\windows';
    $versionedExe = $binDir . '\releases\RealServer_' . $version . '.exe';
    if (!is_file($versionedExe)) {
        gu_out(['ok' => false, 'error' => 'artifact_missing', 'path' => $versionedExe], 400);
    }

    redis_hash_set($r, PROC_UPDATE_CFG_KEY, [
        'version'    => $version,
        'batch_size' => (string)$batchSize,
        'status'     => 'rolling',
        'note'       => 'panel gray update ' . date('Y-m-d H:i:s'),
    ]);
    $r->rpush(PROC_UPDATE_LOG_KEY, time() . '|process|panel-roll|' . $version);

    // 同步滚动该类型进程 (batch_size=1 时每进程约 5s, 面板更新按钮会阻塞数秒)
    // 关键: 必须 < NUL > file 2>&1 重定向三路句柄。否则 powershell 从 PHP exec 启动时
    // stdout/stderr 走管道, .NET CLR 初始化会卡 ~30s; 且 gray_roll.ps1 里 Start-Process
    // 拉起的子进程会继承 exec 的 stdout 管道, 导致 exec() 永不返回(面板卡死)。
    $script  = 'E:\RealServer\bin\windows\gray_roll.ps1';
    $logFile = $binDir . '\logs\gray_roll_' . $type . '.log';
    @mkdir(dirname($logFile), 0777, true);
    $cmd = 'powershell -NoProfile -ExecutionPolicy Bypass -File ' . $script
         . ' -Type ' . $type . ' -ExePath ' . $versionedExe . ' -BatchSize ' . $batchSize
         . ' < NUL > ' . $logFile . ' 2>&1';
    $output = [];
    $code = 0;
    exec($cmd, $output, $code);
    $outputText = @file_get_contents($logFile);
    if ($outputText === false) {
        $outputText = implode("\n", $output);
    }

    if ($code !== 0) {
        redis_hash_set($r, PROC_UPDATE_CFG_KEY, [
            'status' => 'paused',
            'note'   => 'roll failed: ' . substr($outputText, 0, 200),
        ]);
        $r->rpush(PROC_UPDATE_LOG_KEY, time() . '|process|roll-failed|' . $version);
        gu_out(['ok' => false, 'error' => 'roll_failed', 'output' => $outputText], 500);
    }

    redis_hash_set($r, PROC_UPDATE_CFG_KEY, [
        'status' => 'done',
        'note'   => 'rolled type ' . $type . ' to ' . $version,
    ]);
    $r->rpush(PROC_UPDATE_LOG_KEY, time() . '|process|rolled|' . $version);

    gu_out(['ok' => true, 'version' => $version, 'type' => $type, 'output' => $outputText]);
}

gu_out(['ok' => false, 'error' => 'method_not_allowed'], 405);
