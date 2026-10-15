<?php
// =============================================================================
// gray_update.php — 灰度更新面板后端（session 鉴权，供 gray_update.php 前端 fetch）
//
// GET  ?type=<0|100|101|102|201|202>[&target=<version>]
//   返回该类型可用构建版本列表 + 进程列表(含 latest/旧版本 判定) + 统计四项。
// POST {type, version, batch_size}
//   写 cluster:update:process:config(rolling) → 同步调 gray_roll.ps1 -Type <type> 分批
//   滚动重启 → 成功 status=done / 失败 status=paused，并记 process log。
// =============================================================================
require_once __DIR__ . '/../config.php';

// 灰度滚动脚本分批重启进程耗时远超默认 max_execution_time=30s, 且历史实现里
// exec() 会等待 gray_roll.ps1 里 Start-Process 拉起的子进程句柄, 导致「点击卡死」。
// 这里放开执行时限并忽略客户端中断; 真正的 fire-and-forget 见下方 POST 分支的异步触发。
set_time_limit(0);
ignore_user_abort(true);

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
    202 => 'Battle',
];

// 「所有进程」(type=0) 列表聚合的类型 = 全部类型 (含 Gate, 仅供查看)。
// 滚动更新时 Gate 仍被排除 (POST 分支 -ExcludeType 100): Gate 是特殊操作(客户端直接断链),
// 由面板上的独立「Gate 灰度更新」按钮经 type=100 单独触发, 不混入「所有进程」常规滚动。
const GU_LIST_TYPES = [100, 101, 102, 201, 202];

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
    // type=0 表示「所有进程」, 聚合 GU_LIST_TYPES (含 Gate 查看); 否则必须是合法单类型。
    if ($type !== 0 && !isset(GU_TYPE_NAMES[$type])) {
        gu_out(['ok' => false, 'error' => 'bad_type'], 400);
    }
    $typeName  = $type === 0 ? '所有进程' : GU_TYPE_NAMES[$type];
    $scanTypes = $type === 0 ? GU_LIST_TYPES : [$type];

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
        foreach ($scanTypes as $t) {
            $members = $r->smembers("cluster:process:by_type:{$t}");
            if (!is_array($members)) {
                continue;
            }
            sort($members, SORT_NUMERIC);
            foreach ($members as $id) {
                $h = $r->hgetall("cluster:process:{$t}:{$id}");
                if (!is_array($h) || empty($h)) {
                    continue;
                }
                $ver = (string)($h['version'] ?? '');
                $processes[] = [
                    'id'              => (string)$id,
                    'type'            => (string)$t,
                    'type_name'       => GU_TYPE_NAMES[$t] ?? "Type{$t}",
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

    // 灰度滚动状态: 前端据此自动刷新进度徽标 (rolling=滚动中 / 其它=空闲)。
    $cfg = ['status' => '', 'version' => '', 'note' => ''];
    try {
        $raw = $r->hgetall(PROC_UPDATE_CFG_KEY);
        if (is_array($raw)) {
            $cfg = [
                'status'  => (string)($raw['status'] ?? ''),
                'version' => (string)($raw['version'] ?? ''),
                'note'    => (string)($raw['note'] ?? ''),
            ];
        }
    } catch (Throwable $e) {
    }

    gu_out([
        'ok'        => true,
        'type'      => $type,
        'type_name' => $typeName,
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
        'updating'  => ($cfg['status'] === 'rolling'),
        'cfg'       => $cfg,
        'now'       => time(),
    ]);
}

// ---------- POST: 触发灰度滚动 ----------
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $type      = (int)($_POST['type'] ?? 0);
    $version   = trim((string)($_POST['version'] ?? ''));
    $batchSize = max(1, (int)($_POST['batch_size'] ?? 1));

    // type=0 表示「所有进程(除 Gate)」; 否则必须是合法单类型。
    if ($type !== 0 && !isset(GU_TYPE_NAMES[$type])) {
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

    // 异步触发 (fire-and-forget) —— 这是「点击卡死 / 提示更新失败」的根因修复。
    // 历史实现用 exec() 同步等待滚动脚本: 分批重启进程耗时远超 max_execution_time,
    // 且 gray_roll.ps1 里 Start-Process 拉起的子进程会继承 exec 的 stdout 管道句柄,
    // 使 exec() 永不返回, 单 worker 的 PHP built-in server 因此被占死。
    // 这里改为后台启动后立即返回 queued, 面板改为轮询 GET 统计/进程列表查看进度。
    $script  = 'E:\RealServer\bin\windows\gray_roll.ps1';
    $logFile = $binDir . '\logs\gray_roll_' . ($type === 0 ? 'all' : $type) . '.log';
    @mkdir(dirname($logFile), 0777, true);
    // type=0(所有进程)滚动时排除 Gate (客户端直接断链); Gate 由独立按钮 type=100 单独触发。
    $excludeArg = ($type === 0) ? ' -ExcludeType 100' : '';
    $ps = 'powershell -NoProfile -ExecutionPolicy Bypass -File "' . $script
        . '" -Type ' . $type . $excludeArg . ' -ExePath "' . $versionedExe . '" -BatchSize ' . $batchSize;
    // 三路句柄重定向: 滚动子进程不继承 PHP 的 stdout/stderr 管道, 输出落到日志文件。
    $cmd = 'cmd /c "' . $ps . ' > "' . $logFile . '" 2>&1"';

    $launched = false;
    // 首选: WScript.Shell 后台运行 (0=隐藏窗口, false=不等待)。
    if (class_exists('COM')) {
        try {
            $sh = new COM('WScript.Shell');
            $sh->Run($cmd, 0, false);
            $launched = true;
        } catch (Throwable $e) {
            $launched = false;
        }
    }
    // 兜底: popen + start /b (cmd 后台执行, popen 立即返回)。
    if (!$launched) {
        pclose(popen('start /b "" ' . $cmd, 'r'));
        $launched = true;
    }

    gu_out([
        'ok'      => true,
        'version' => $version,
        'type'    => $type,
        'job'     => 'queued',
        'log'     => $logFile,
    ]);
}

gu_out(['ok' => false, 'error' => 'method_not_allowed'], 405);
