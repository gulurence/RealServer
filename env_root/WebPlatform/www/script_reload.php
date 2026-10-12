<?php
// =============================================================================
// 脚本热更 — 按脚本目录展示多级文件列表, 标记是否有变动, 勾选后发布到 Redis,
// 各进程订阅 cluster:gm:all 的 hotfix 命令 → 拉取文件内容写回 script/ → LuaHotFix 热更。
//
// Redis 键约定:
//   cluster:hotfix:pkg:<v>   STRING 二进制包 "HFX1"+version+count+每文件[pathLen+path+contentLen+content]
//                              (WebPlatform 打包「MD5 发生变化的文件」, 进程解包后按内存内容热更)
//   cluster:hotfix:files     HASH  {relpath} → 文件内容 (最近一次发布内容, 供回滚取 before)
//   cluster:hotfix:baseline  HASH  {relpath} → md5 (热更后基线, 用于「是否有变动」)
//   cluster:hotfix:history   LIST  JSON {seq,ts,operator,action,files:[{path,before,after}]}
//   cluster:hotfix:seq       INCR  (单调版本号)
//   命令通道                 cluster:gm:all → "hotfix <version>"
// =============================================================================
require_once __DIR__ . '/_layout.php';

const SCRIPT_ROOT = __DIR__ . '/../../../script';
const HF_PKG_KEY = 'cluster:hotfix:pkg:';
const HF_FILES_KEY = 'cluster:hotfix:files';
const HF_BASELINE_KEY = 'cluster:hotfix:baseline';
const HF_HISTORY_KEY = 'cluster:hotfix:history';
const HF_SEQ_KEY = 'cluster:hotfix:seq';
const HF_CHANNEL = 'cluster:gm:all';
const HF_HISTORY_MAX = 200;

// 框架核心文件: 定义热更机制本身 / 持有 file-local 注册表, 热更会导致断链, 标灰需整机重启。
function is_framework_file(string $rel): bool
{
    static $fw = [
        'common/class.lua',
        'common/include.lua',
        'common/hotfix.lua',
        'common/actor/actor_mgr.lua',
        'common/actor/actor_base.lua',
        'common/actor/actor_creator.lua',
        'common/actor/include.lua',
    ];
    return in_array($rel, $fw, true);
}

/** 递归扫描 script/ 下所有 .lua, 返回按路径排序的 relpath 列表 (统一 / 分隔)。 */
function scan_scripts(string $root): array
{
    $files = [];
    $it = new RecursiveIteratorIterator(
        new RecursiveDirectoryIterator($root, FilesystemIterator::SKIP_DOTS)
    );
    foreach ($it as $f) {
        if (!$f->isFile()) continue;
        if (strtolower($f->getExtension()) !== 'lua') continue;
        $rel = str_replace('\\', '/', substr($f->getPathname(), strlen($root) + 1));
        $files[] = $rel;
    }
    sort($files, SORT_STRING);
    return $files;
}

/** 校验 relpath 安全 (仅 .lua, 且必须落在 SCRIPT_ROOT 内), 返回真实内容或 null。 */
function read_script(string $root, string $rel): ?string
{
    if (strtolower(pathinfo($rel, PATHINFO_EXTENSION)) !== 'lua') return null;
    if (preg_match('#^[a-zA-Z0-9_./-]+$#', $rel) !== 1) return null;
    $abs = realpath($root . '/' . $rel);
    $base = realpath($root);
    if ($abs === false || $base === false || strpos($abs, $base) !== 0) return null;
    $c = file_get_contents($abs);
    return $c === false ? null : $c;
}

/**
 * 打包二进制热更数据 (与 C++ GmCommandMgr::UnpackHotfixPackage 对齐):
 *   "HFX1" + version(u32 LE) + count(u32 LE) + 每文件[pathLen(u32 LE)+path+contentLen(u32 LE)+content]
 * $files 形如 [[relpath, content], ...]。
 */
function build_hotfix_package(int $ver, array $files): string
{
    $bin = 'HFX1' . pack('V', $ver) . pack('V', count($files));
    foreach ($files as $f) {
        $path = (string)$f[0];
        $content = (string)$f[1];
        $bin .= pack('V', strlen($path)) . $path;
        $bin .= pack('V', strlen($content)) . $content;
    }
    return $bin;
}

/** 拉取所有在线进程的热更状态 (lua 版本 / 上次热更结果 / 报错 / 已热更版本列表)。 */
function fetch_hotfix_process_status(?RedisCluster $r): array
{
    $result = [];
    if ($r === null) {
        return $result;
    }
    foreach (array_keys(PROCESS_TYPES) as $type) {
        $members = $r->smembers("cluster:process:by_type:{$type}");
        if (!is_array($members)) {
            continue;
        }
        foreach ($members as $id) {
            $h = $r->hgetall("cluster:process:{$type}:{$id}");
            if (!is_array($h) || empty($h)) {
                continue;
            }
            if (!is_online($h)) {
                continue;
            }
            $h['type'] = (string)$type;
            $result[] = $h;
        }
    }
    return $result;
}

$r = redis();
$operator = $_SESSION['user'] ?? 'admin';

$notice = '';
$noticeOk = true;

// ---------------------------------------------------------------------------
// POST: 热更 或 回滚
// ---------------------------------------------------------------------------
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $action = post_str('action', 'hotfix');

    if ($r === null) {
        $notice = '无法连接 Redis，操作未执行';
        $noticeOk = false;
    } elseif ($action === 'rollback') {
        $seq = post_str('seq', '');
        $raw = $r->lrange(HF_HISTORY_KEY, 0, -1);
        $entry = null;
        if (is_array($raw)) {
            foreach ($raw as $line) {
                $e = json_decode((string)$line, true);
                if (is_array($e) && (string)($e['seq'] ?? '') === $seq) {
                    $entry = $e;
                    break;
                }
            }
        }
        if ($entry === null) {
            $notice = '回滚失败：未找到该历史条目';
            $noticeOk = false;
        } else {
            $packFiles = [];
            $restored = 0;
            foreach (($entry['files'] ?? []) as $f) {
                $p = (string)($f['path'] ?? '');
                $before = $f['before'] ?? null;
                if ($p === '' || $before === null || $before === '') continue;
                $content = base64_decode((string)$before, true);
                if ($content === false) continue;
                $packFiles[] = [$p, $content];
                $restored++;
            }
            if ($restored === 0) {
                $notice = '回滚失败：该条目无可回滚的文件（发布前内容缺失）';
                $noticeOk = false;
            } else {
                $ver = $r->incr(HF_SEQ_KEY);
                $r->set(HF_PKG_KEY . $ver, build_hotfix_package((int)$ver, $packFiles));
                foreach ($packFiles as $f) {
                    $r->hset(HF_FILES_KEY, $f[0], $f[1]);
                    $r->hset(HF_BASELINE_KEY, $f[0], md5($f[1]));
                }
                $newEntry = [
                    'seq' => $ver, 'ts' => time(), 'operator' => $operator,
                    'action' => 'rollback', 'rollback_of' => $seq,
                    'files' => array_map(function ($f) {
                        return ['path' => $f[0], 'before' => null, 'after' => base64_encode($f[1])];
                    }, $packFiles),
                ];
                $r->rpush(HF_HISTORY_KEY, json_encode($newEntry, JSON_UNESCAPED_UNICODE));
                $r->ltrim(HF_HISTORY_KEY, -HF_HISTORY_MAX, -1);
                $cmd = 'hotfix ' . $ver;
                $n = $r->publish(HF_CHANNEL, $cmd);
                $notice = "已回滚 <b>{$restored}</b> 个文件（seq={$seq} → v{$ver}），收到订阅进程数：{$n}";
            }
        }
    } else {
        // 热更: 打包所有「MD5 相对基线发生变化」的选中文件 → SET pkg:<v> → PUBLISH "hotfix <v>"
        $files = $_POST['files'] ?? [];
        if (!is_array($files) || empty($files)) {
            $notice = '未选择任何文件';
            $noticeOk = false;
        } else {
            $pushed = 0;
            $entryFiles = [];
            $packFiles = [];
            foreach ($files as $rel) {
                $rel = str_replace('\\', '/', (string)$rel);
                if (is_framework_file($rel)) continue;   // 框架文件不参与热更
                $content = read_script(SCRIPT_ROOT, $rel);
                if ($content === null) continue;
                $newMd5 = md5($content);
                // 仅打包 MD5 相对基线发生变化的文件 (含从未发布), 未变化的跳过。
                $base = $r->hget(HF_BASELINE_KEY, $rel);
                if ($base !== false && (string)$base === $newMd5) continue;
                $before = $r->hget(HF_FILES_KEY, $rel);   // 上次发布内容 (false=从未发布)
                $packFiles[] = [$rel, $content];
                $entryFiles[] = [
                    'path' => $rel,
                    'before' => $before === false ? null : base64_encode((string)$before),
                    'after' => base64_encode($content),
                ];
                $pushed++;
            }
            if ($pushed === 0) {
                $notice = '无可发布文件（未选 / 框架文件 / 读取失败 / MD5 未变化）';
                $noticeOk = false;
            } else {
                $ver = $r->incr(HF_SEQ_KEY);
                // 写二进制包 (进程侧 UnpackHotfixPackage 解包后按内存内容热更)。
                $r->set(HF_PKG_KEY . $ver, build_hotfix_package((int)$ver, $packFiles));
                // 记录发布内容 (baseline md5 + files 内容, 供界面变动判定/回滚取 before)。
                foreach ($packFiles as $f) {
                    $r->hset(HF_FILES_KEY, $f[0], $f[1]);
                    $r->hset(HF_BASELINE_KEY, $f[0], md5($f[1]));
                }
                $entry = [
                    'seq' => $ver, 'ts' => time(), 'operator' => $operator,
                    'action' => 'hotfix', 'files' => $entryFiles,
                ];
                $r->rpush(HF_HISTORY_KEY, json_encode($entry, JSON_UNESCAPED_UNICODE));
                $r->ltrim(HF_HISTORY_KEY, -HF_HISTORY_MAX, -1);
                $cmd = 'hotfix ' . $ver;
                $n = $r->publish(HF_CHANNEL, $cmd);
                $notice = "已打包并发布 <b>{$pushed}</b> 个 MD5 变化的文件（v{$ver}）到 <code>{$cmd}</code>，收到订阅进程数：{$n}";
            }
        }
    }
}

// ---------------------------------------------------------------------------
// 读当前状态: 基线 (md5) + 历史
// ---------------------------------------------------------------------------
$baseline = redis_hash_get($r, HF_BASELINE_KEY);

$history = [];
if ($r !== null) {
    $raw = $r->lrange(HF_HISTORY_KEY, 0, -1);
    if (is_array($raw)) {
        foreach (array_reverse($raw) as $line) {   // 最新在前
            $e = json_decode((string)$line, true);
            if (is_array($e)) $history[] = $e;
        }
    }
}

// 文件列表 + 变动判定 (md5 对所有文件计算, 供界面展示 + 「有变动」判定)
$rows = [];
foreach (scan_scripts(SCRIPT_ROOT) as $rel) {
    $c = read_script(SCRIPT_ROOT, $rel);
    $md5 = $c !== null ? md5($c) : null;
    $hasBase = array_key_exists($rel, $baseline);
    $changed = $hasBase && $md5 !== null && $md5 !== (string)$baseline[$rel];
    $rows[] = [
        'rel' => $rel, 'depth' => substr_count($rel, '/'),
        'md5' => $md5, 'changed' => $changed, 'published' => $hasBase,
        'fw' => is_framework_file($rel),
    ];
}

/**
 * 把扁平文件行列表构建成嵌套目录树: node = ['dirs'=>[name=>node], 'files'=>[row,...]]。
 * 用于把文件列表按文件夹层级折叠展示 (上级折叠即隐藏下级)。
 */
function build_hotfix_tree(array $rows): array
{
    $root = ['dirs' => [], 'files' => []];
    foreach ($rows as $row) {
        $parts = explode('/', $row['rel']);
        $node = &$root;
        $n = count($parts);
        for ($i = 0; $i < $n - 1; $i++) {
            $seg = $parts[$i];
            if (!isset($node['dirs'][$seg])) {
                $node['dirs'][$seg] = ['dirs' => [], 'files' => []];
            }
            $node = &$node['dirs'][$seg];
        }
        $node['files'][] = $row;
        unset($node);
    }
    return $root;
}

/** 递归渲染一个目录: 折叠头 + 内含(子目录递归 + 本目录文件)的容器。 */
function render_hotfix_dir(array $node, string $path, int $depth): void
{
    $dirId = 'dir-' . substr(md5($path), 0, 12);
    ?>
    <div class="dir-header d-flex align-items-center gap-1 text-secondary" role="button"
         data-target="#<?= $dirId ?>" style="padding-left:<?= $depth * 16 ?>px;margin-top:6px;cursor:pointer;user-select:none">
        <i class="ti ti-chevron-down dir-caret"></i>
        <i class="ti ti-folder"></i>
        <span><?= esc(basename($path) . '/') ?></span>
    </div>
    <div class="dir-files" id="<?= $dirId ?>">
    <?php
    $subdirs = $node['dirs'];
    ksort($subdirs, SORT_STRING);
    foreach ($subdirs as $name => $child) {
        render_hotfix_dir($child, $path . '/' . $name, $depth + 1);
    }
    render_hotfix_files($node['files'], $depth + 1);
    echo '</div>';
}

/** 渲染某目录下的文件行 (复选框 + 文件名 + md5 + 状态徽标)。 */
function render_hotfix_files(array $files, int $depth): void
{
    foreach ($files as $row):
        ?>
        <label class="d-flex align-items-center gap-2"
               style="padding-left:<?= $depth * 16 ?>px; <?= $row['fw'] ? 'opacity:.5' : '' ?>">
            <input class="form-check-input hotfix-file" type="checkbox" name="files[]"
                   value="<?= esc($row['rel']) ?>" <?= $row['fw'] ? 'disabled' : '' ?>
                   <?= (!$row['fw'] && $row['changed']) ? 'data-changed="1"' : '' ?>>
            <span><?= esc(basename($row['rel'])) ?></span>
            <span class="mono small text-secondary" title="<?= esc($row['md5'] ?? '') ?>"><?= esc(substr($row['md5'] ?? '', 0, 8)) ?></span>
            <?php if ($row['fw']): ?>
                <span class="badge bg-dark">框架·需重启</span>
            <?php elseif ($row['changed']): ?>
                <span class="badge bg-yellow">有变动</span>
            <?php elseif (!$row['published']): ?>
                <span class="badge bg-secondary">未发布过</span>
            <?php endif; ?>
        </label>
        <?php
    endforeach;
}

layout_header('脚本热更', 'script_reload');
?>
<?php if ($notice !== ''): ?>
    <div class="alert <?= $noticeOk ? 'alert-success' : 'alert-warning' ?>"><?= $notice ?></div>
<?php endif; ?>

<?php if ($r === null): ?>
    <div class="card"><div class="card-body"><div class="empty">⚠️ 无法连接 Redis Cluster。</div></div></div>
<?php else: ?>

<form method="post" class="form">
    <input type="hidden" name="action" value="hotfix">
    <div class="card">
        <div class="card-header">
            脚本文件列表 <span class="dim">script/ 下全部 .lua，勾选后底部「热更」发布到 Redis</span>
            <span class="ms-auto d-inline-flex gap-2">
                <button type="button" class="btn btn-sm" id="sel-changed">全选有变动</button>
                <button type="button" class="btn btn-sm" id="sel-none">取消全选</button>
                <button type="button" class="btn btn-sm" id="fold-all">全部展开</button>
                <button type="button" class="btn btn-sm" id="unfold-all">全部折叠</button>
            </span>
        </div>
        <div class="card-body">
            <div class="dim mb-2">图例：<span class="badge bg-yellow">有变动</span>
                <span class="badge bg-secondary">未发布过</span>
                <span class="badge bg-dark">框架·需重启</span></div>
            <div id="file-tree" style="font-family:var(--tblr-font-monospace,monospace);font-size:13px;line-height:1.9">
            <?php
            $tree = build_hotfix_tree($rows);
            $topDirs = $tree['dirs'];
            ksort($topDirs, SORT_STRING);
            foreach ($topDirs as $name => $child) {
                render_hotfix_dir($child, $name, 0);
            }
            render_hotfix_files($tree['files'], 0);
            ?>
            </div>
        </div>
        <div class="card-footer">
            <button class="btn btn-primary" type="submit"><i class="ti ti-package"></i> 热更</button>
            <span class="dim ms-2">将所选文件内容写入 Redis 并向全部进程发布 hotfix 命令</span>
        </div>
    </div>
</form>

<div class="card mt-3">
    <div class="card-header">热更历史 <span class="dim">最新在前，回滚 = 重新发布该次热更前的文件内容</span></div>
    <div class="card-body">
        <?php if (empty($history)): ?>
            <div class="empty">暂无热更记录。</div>
        <?php else: ?>
        <table class="table table-sm">
            <thead><tr><th>时间</th><th>版本</th><th>操作</th><th>文件</th><th style="width:80px">回滚</th></tr></thead>
            <tbody>
            <?php foreach ($history as $e): ?>
                <tr>
                    <td class="text-nowrap"><?= esc(date('Y-m-d H:i:s', (int)($e['ts'] ?? 0))) ?></td>
                    <td>v<?= esc((string)($e['seq'] ?? '?')) ?></td>
                    <td><?= esc(($e['action'] ?? '') === 'rollback' ? '回滚' : '热更') ?>
                        <?= isset($e['rollback_of']) ? ('<span class="dim">(of v' . esc((string)$e['rollback_of']) . ')</span>') : '' ?>
                        <div class="dim small"><?= esc((string)($e['operator'] ?? '')) ?></div></td>
                    <td class="mono small"><?= esc(implode(', ', array_column($e['files'] ?? [], 'path'))) ?></td>
                    <td>
                        <?php if (($e['action'] ?? '') === 'hotfix'): ?>
                        <form method="post" onsubmit="return confirm('确认回滚该次热更？将重新发布热更前的脚本内容。')">
                            <input type="hidden" name="action" value="rollback">
                            <input type="hidden" name="seq" value="<?= esc((string)($e['seq'] ?? '')) ?>">
                            <button class="btn btn-sm btn-outline-warning" type="submit">回滚</button>
                        </form>
                        <?php endif; ?>
                    </td>
                </tr>
            <?php endforeach; ?>
            </tbody>
        </table>
        <?php endif; ?>
    </div>
</div>

<div class="card mt-3">
    <div class="card-header">进程热更状态 <span class="dim">各进程当前 lua 版本 / 上次热更结果与报错</span></div>
    <div class="card-body">
        <?php $procs = fetch_hotfix_process_status($r); ?>
        <?php if (empty($procs)): ?>
            <div class="empty">暂无在线进程。</div>
        <?php else: ?>
        <table class="table table-sm">
            <thead><tr><th>进程</th><th>类型</th><th>Lua版本</th><th>热更状态</th><th>已热更版本</th><th>报错信息</th></tr></thead>
            <tbody>
            <?php foreach ($procs as $p): ?>
                <tr>
                    <td class="mono"><?= esc($p['name'] ?? $p['id'] ?? '') ?></td>
                    <td><?= esc(process_type_name($p['type'] ?? '')) ?></td>
                    <td class="mono"><?= esc($p['lua_version'] ?? '—') ?></td>
                    <td>
                        <?php
                        $hs = $p['hotfix_status'] ?? '';
                        if ($hs === '') {
                            echo '<span class="dim">—</span>';
                        } else {
                            $badge = $hs === 'ok' ? 'text-bg-success' : ($hs === 'precheck_ok' ? 'text-bg-info' : 'text-bg-warning');
                            echo '<span class="badge ' . $badge . '">' . esc($hs) . '</span>';
                        }
                        ?>
                    </td>
                    <td class="mono small"><?= esc($p['hotfix_versions'] ?? '—') ?></td>
                    <td class="small text-danger" style="white-space:pre-line"><?= esc($p['hotfix_error'] ?? '') ?></td>
                </tr>
            <?php endforeach; ?>
            </tbody>
        </table>
        <?php endif; ?>
    </div>
</div>

<script>
(function () {
    var boxes = document.querySelectorAll('.hotfix-file:not([disabled])');
    var changed = document.querySelectorAll('.hotfix-file[data-changed="1"]');
    document.getElementById('sel-changed').addEventListener('click', function () {
        boxes.forEach(function (b) { b.checked = false; });
        changed.forEach(function (b) { b.checked = true; });
    });
    document.getElementById('sel-none').addEventListener('click', function () {
        boxes.forEach(function (b) { b.checked = false; });
    });

    // 目录折叠/展开: 点击目录头切换其子文件显示
    function setFold(header, folded) {
        var target = document.querySelector(header.getAttribute('data-target'));
        if (target) { target.style.display = folded ? 'none' : ''; }
        var caret = header.querySelector('.dir-caret');
        if (caret) {
            caret.classList.toggle('ti-chevron-down', !folded);
            caret.classList.toggle('ti-chevron-right', folded);
        }
    }
    document.querySelectorAll('.dir-header').forEach(function (h) {
        h.addEventListener('click', function () {
            var target = document.querySelector(h.getAttribute('data-target'));
            var folded = target && target.style.display === 'none';
            setFold(h, !folded);
        });
    });
    document.getElementById('fold-all').addEventListener('click', function () {
        document.querySelectorAll('.dir-header').forEach(function (h) { setFold(h, false); });
    });
    document.getElementById('unfold-all').addEventListener('click', function () {
        document.querySelectorAll('.dir-header').forEach(function (h) { setFold(h, true); });
    });
})();
</script>

<?php endif; ?>

<?php layout_footer(); ?>
