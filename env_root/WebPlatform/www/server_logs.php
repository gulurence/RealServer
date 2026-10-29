<?php
// =============================================================================
// server_logs.php — 服务器进程日志实时查看
//
// 读取 RealServer 运行时的进程日志 (bin/windows/logs/*.out / *.err / normal*.log),
// 下拉选择要查看的日志文件, 自动刷新 (默认每 3 秒) 显示文件末尾最新若干行。
// 日志文件命名与 bin/windows/start_cluster.ps1 的进程命名一致:
//   gate-01 (100) / login-01 (101) / global-01 (102) / game-01 (201)。
// =============================================================================
require_once __DIR__ . '/_layout.php';

const TAIL_LINES    = 300;      // 显示末尾行数
const TAIL_MAXBYTES = 524288;   // 尾部读取窗口 (字节, 足够 300 行典型日志)
const LOG_LEVELS    = ['trace', 'debug', 'info', 'warn', 'error', 'critical'];   // 日志等级下拉选项 (spdlog)

// ---- 日志文件名 → 友好显示名 (进程名映射与 start_cluster.ps1 一致) ----
function log_label(string $f): string
{
    static $proc = ['gate' => 'Gate', 'login' => 'Login', 'global' => 'Global', 'game' => 'Game'];
    if (preg_match('/^([a-z]+)-\d+\.(out|err)$/i', $f, $m)) {
        $name = $proc[strtolower($m[1])] ?? ucfirst($m[1]);
        return $name . ' 进程 — ' . $f . ' (' . ($m[2] === 'out' ? 'stdout' : 'stderr') . ')';
    }
    if (preg_match('/^normal(\.\d+)?\.log$/i', $f)) {
        return '通用日志 — ' . $f;
    }
    return $f;
}

// ---- 枚举日志目录下可查看的日志文件 (仅取 basename, 杜绝路径穿越) ----
function list_log_files(string $dir): array
{
    $map = [];
    foreach (['*.out', '*.err', 'normal*.log'] as $p) {
        foreach (glob(rtrim($dir, '/\\') . DIRECTORY_SEPARATOR . $p) ?: [] as $path) {
            $b = basename($path);
            $map[$b] = log_label($b);
        }
    }
    ksort($map);
    return $map;
}

// ---- 读文件末尾 N 行 (只读尾部窗口, 截断首行避免半行) ----
function read_tail(string $path, int $lines, int $maxBytes): string
{
    if (!is_file($path)) return '';
    $size = @filesize($path);
    if ($size === false || $size === 0) return '';
    $fp = @fopen($path, 'rb');
    if (!$fp) return '';
    $offset = max(0, $size - $maxBytes);
    if ($offset > 0) {
        @fseek($fp, $offset);
        @fgets($fp);   // 丢弃从 offset 起的第一行 (可能是半行)
    }
    $chunk = stream_get_contents($fp);
    @fclose($fp);
    if ($chunk === false || $chunk === '') return '';
    $all = explode("\n", $chunk);
    return implode("\n", array_slice($all, max(0, count($all) - $lines)));
}

// ---- 按 spdlog 级别给行着色 ([info]/[warn]/[error]/[critical]) ----
function render_log_tail(string $text): string
{
    if ($text === '') return '';
    $out = [];
    foreach (explode("\n", $text) as $line) {
        $l = strtolower($line);
        $cls = '';
        if (str_contains($l, '[error]') || str_contains($l, '[critical]')) $cls = 'lvl-error';
        elseif (str_contains($l, '[warn]')) $cls = 'lvl-warn';
        elseif (str_contains($l, '[info]')) $cls = 'lvl-info';
        $e = esc($line);
        $out[] = $cls !== '' ? '<span class="' . $cls . '">' . $e . '</span>' : $e;
    }
    return implode("\n", $out);
}

// ---- 按日志等级过滤行: 仅保留含 "[level]" 标记的行 (不区分大小写) ----
function filter_log_level(string $text, string $level): string
{
    if ($text === '' || $level === '') return $text;
    $needle = '[' . strtolower($level) . ']';
    $out = [];
    foreach (explode("\n", $text) as $line) {
        if (str_contains(strtolower($line), $needle)) $out[] = $line;
    }
    return implode("\n", $out);
}

// ---- 按日志内容关键字过滤行 (不区分大小写的子串匹配) ----
function filter_log_content(string $text, string $q): string
{
    if ($text === '' || $q === '') return $text;
    $needle = strtolower($q);
    $out = [];
    foreach (explode("\n", $text) as $line) {
        if (str_contains(strtolower($line), $needle)) $out[] = $line;
    }
    return implode("\n", $out);
}

$logDir   = REALSERVER_LOG_DIR;
$logFiles = is_dir($logDir) ? list_log_files($logDir) : [];

$selected = (string)($_GET['log'] ?? '');
if ($selected === '' || !isset($logFiles[$selected])) {
    // 默认优先显示有内容的日志: 通用日志 normal.log → 各进程 stdout
    $selected = '';
    foreach (['normal.log', 'gate-01.out', 'login-01.out', 'global-01.out', 'game-01.out'] as $name) {
        $p = $logDir . DIRECTORY_SEPARATOR . $name;
        if (isset($logFiles[$name]) && @filesize($p) > 0) { $selected = $name; break; }
    }
    if ($selected === '') $selected = (string)(array_key_first($logFiles) ?? '');
}

// 日志等级过滤 (''=全部): 仅保留含 "[level]" 标记的行
$level = (string)($_GET['level'] ?? '');
if ($level !== '' && !in_array($level, LOG_LEVELS, true)) $level = '';

// 日志内容查询 (''=全部): 仅保留包含关键字的行 (不区分大小写)
$q = trim((string)($_GET['q'] ?? ''));

$logPath  = $logFiles !== [] ? $logDir . DIRECTORY_SEPARATOR . $selected : '';
$tailText = $logPath !== '' ? read_tail($logPath, TAIL_LINES, TAIL_MAXBYTES) : '';
if ($level !== '' && $tailText !== '') {
    $tailText = filter_log_level($tailText, $level);
}
if ($q !== '' && $tailText !== '') {
    $tailText = filter_log_content($tailText, $q);
}
$logSize  = $logPath !== '' ? (int)(@filesize($logPath) ?: 0) : 0;
$logMtime = $logPath !== '' ? (int)(@filemtime($logPath) ?: 0) : 0;

// 空态提示 (区分 无内容 / 等级过滤 / 内容过滤 / 组合过滤)
$emptyMsg = '暂无日志内容（文件为空或尚未产生输出）。';
if ($tailText === '') {
    if ($q !== '' && $level !== '') {
        $emptyMsg = '没有匹配「' . $q . '」且日志等级为「' . $level . '」的日志行（当前文件末尾 ' . TAIL_LINES . ' 行内）。';
    } elseif ($q !== '') {
        $emptyMsg = '没有匹配「' . $q . '」的日志行（当前文件末尾 ' . TAIL_LINES . ' 行内）。';
    } elseif ($level !== '') {
        $emptyMsg = '该日志等级「' . $level . '」暂无匹配行（当前文件末尾 ' . TAIL_LINES . ' 行内）。';
    }
}

layout_header('服务器进程日志', 'server_logs');
?>

<?php if ($logFiles === []): ?>
    <div class="empty">⚠️ 未找到日志目录 <code><?= esc($logDir) ?></code>。请先通过 <code>bin/windows/start_cluster.ps1</code> 启动 RealServer。</div>
<?php else: ?>

<?php if (!is_partial()): ?>
<!-- 选择器卡片 (自动刷新时不重置) -->
<div class="card mb-3">
    <div class="card-body">
        <div class="d-flex align-items-center gap-3 flex-wrap">
            <label class="form-label mb-0" for="log-select">日志文件</label>
            <select id="log-select" class="form-select" style="max-width:460px">
                <?php foreach ($logFiles as $f => $label): ?>
                    <option value="<?= esc($f) ?>"<?= $f === $selected ? ' selected' : '' ?>><?= esc($label) ?></option>
                <?php endforeach; ?>
            </select>
            <label class="form-label mb-0" for="level-select">日志等级</label>
            <select id="level-select" class="form-select" style="max-width:180px">
                <option value=""<?= $level === '' ? ' selected' : '' ?>>全部</option>
                <?php foreach (LOG_LEVELS as $lv): ?>
                    <option value="<?= esc($lv) ?>"<?= $level === $lv ? ' selected' : '' ?>><?= esc($lv) ?></option>
                <?php endforeach; ?>
            </select>
            <form method="get" action="server_logs.php" class="d-flex align-items-center gap-2" style="margin:0">
                <input type="hidden" name="log" value="<?= esc($selected) ?>">
                <input type="hidden" name="level" value="<?= esc($level) ?>">
                <label class="form-label mb-0" for="log-q">日志内容</label>
                <input type="text" id="log-q" name="q" value="<?= esc($q) ?>" class="form-control" style="max-width:220px" placeholder="关键字（匹配日志行内容）">
                <button type="submit" class="btn btn-sm btn-primary">查询</button>
                <?php if ($q !== ''): ?><a class="btn btn-sm btn-link" href="server_logs.php?log=<?= urlencode($selected) ?>&level=<?= urlencode($level) ?>">清除</a><?php endif; ?>
            </form>
            <span class="dim">共 <?= count($logFiles) ?> 个日志 · 显示末尾 <?= TAIL_LINES ?> 行</span>
        </div>
    </div>
</div>
<?php endif; ?>

<?php autorefresh(3, '立即刷新', true); ?>
<?php live_region_start(); ?>

<div class="card">
    <div class="card-header d-flex align-items-center gap-3 flex-wrap">
        <strong><?= esc($logFiles[$selected] ?? $selected) ?></strong>
        <span class="dim mono"><?= esc($logPath) ?></span>
        <span class="dim"><?= $logSize > 0 ? number_format($logSize) . ' 字节' : '空文件' ?> · <?= $logMtime > 0 ? date('Y-m-d H:i:s', $logMtime) : '—' ?></span>
        <?php if ($level !== ''): ?><span class="badge">等级: <?= esc($level) ?></span><?php endif; ?>
        <?php if ($q !== ''): ?><span class="badge">内容: <?= esc($q) ?></span><?php endif; ?>
    </div>
    <div class="card-body p-0">
        <?php if ($tailText === ''): ?>
            <div class="empty"><?= esc($emptyMsg) ?></div>
        <?php else: ?>
            <pre id="log-view" class="log-view"><?= render_log_tail($tailText) ?></pre>
        <?php endif; ?>
    </div>
</div>

<?php live_region_end(); ?>

<?php if (!is_partial()): ?>
<script>
(function () {
    // 下拉切换 → 带 log 参数整页跳转, 选择得以保留
    var sel = document.getElementById('log-select');
    if (sel) sel.addEventListener('change', function () {
        var u = new URL(location.href);
        u.searchParams.set('log', sel.value);
        location.href = u.toString();
    });

    // 日志等级切换 → 带 level 参数整页跳转 (''=全部, 删除参数)
    var lv = document.getElementById('level-select');
    if (lv) lv.addEventListener('change', function () {
        var u = new URL(location.href);
        if (lv.value === '') u.searchParams.delete('level');
        else u.searchParams.set('level', lv.value);
        location.href = u.toString();
    });

    // 自动刷新替换 #live-region 后, 把日志滚动到底部
    var region = document.getElementById('live-region');
    function scrollBottom() {
        var pre = document.getElementById('log-view');
        if (pre) pre.scrollTop = pre.scrollHeight;
    }
    if (region && window.MutationObserver) {
        new MutationObserver(function () { setTimeout(scrollBottom, 0); })
            .observe(region, { childList: true, subtree: true });
    }
    scrollBottom();
})();
</script>
<?php endif; ?>

<?php endif; ?>

<?php layout_footer(); ?>
