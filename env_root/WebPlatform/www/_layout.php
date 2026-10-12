<?php
// =============================================================================
// _layout.php — 共享布局 + 通用 UI 工具 (Tabler 企业级后台模板)
//
// 主题: Tabler (MIT, Bootstrap 5)。通过 jsDelivr CDN 引入 CSS/JS + Tabler 图标。
// 每个后台页面: require 本文件 → layout_header(标题, 激活项) → 内容 → layout_footer()。
// 侧边栏导航分组集中在此, 新增页面只改 nav_groups() 一处。
//
// 注意: 生产环境建议把 @latest 换成固定版本号 (如 @tabler/core@1.0.0-beta24),
//       内网/离线环境则自行把 tabler.min.css / tabler.min.js / 图标字体下载到本地 assets/。
// =============================================================================
require_once __DIR__ . '/config.php';

session_start();
if (empty($_SESSION['user'])) {
    header('Location: index.php');
    exit;
}

const TABLER_CSS  = 'assets/tabler.min.css';
const TABLER_ICON = 'assets/tabler-icons.min.css';
const TABLER_JS   = 'assets/tabler.min.js';

// ---------- 通用转义 / 进程类型常量 ----------
function esc(?string $s): string
{
    return htmlspecialchars((string)$s, ENT_QUOTES, 'UTF-8');
}

const PROCESS_TYPES = [
    100 => 'Gate',
    101 => 'Login',
    102 => 'Global',
    201 => 'Game',
];

const ACTOR_TYPES = [
    100 => 'Gate',
    101 => 'Login',
    102 => 'Friend',
    103 => 'Room',
    104 => 'Chat',
    105 => 'Config',
    201 => 'Player',
    203 => 'Battle',
    204 => 'Spectator',
];

const STATE_NAMES = [
    0 => 'Starting', 1 => 'Init', 2 => 'Running',
    3 => 'Paused', 4 => 'Stopping', 5 => 'Shutdown',
];

const HEARTBEAT_TIMEOUT = 15;   // 心跳超时阈值 (秒), 与 RealServer 判定一致

function state_label(?string $s): string
{
    $i = (int)$s;
    return STATE_NAMES[$i] ?? "Unknown({$s})";
}

function is_online(array $node): bool
{
    $hb = (int)($node['heartbeat'] ?? 0);
    return $hb > 0 && (time() - $hb) <= HEARTBEAT_TIMEOUT;
}

function process_type_name(?string $t): string
{
    $i = (int)$t;
    return PROCESS_TYPES[$i] ?? "Type{$i}";
}

function actor_type_name(?string $t): string
{
    $i = (int)$t;
    return ACTOR_TYPES[$i] ?? "ActorType{$i}";
}

// ---------- 侧边栏导航分组 (key => [标题, 文件名]) ----------
function nav_groups(): array
{
    return [
        '战斗' => [
            'docs_frame_sync' => ['帧同步架构', 'docs_frame_sync.php'],
            'battles'         => ['战斗战报', 'battles.php'],
        ],
        '监控' => [
            'nodes'         => ['节点状态', 'nodes.php'],
            'accounts'      => ['在线账户', 'accounts.php'],
            'db_accounts'   => ['数据库账户', 'db_accounts.php'],
            'account_control' => ['账户控制', 'account_control.php'],
            'trace'         => ['链路追踪', 'trace.php'],
            'actors'        => ['Actor 实例', 'actors.php'],
            'server_logs'   => ['进程日志', 'server_logs.php'],
        ],
        '集群控制' => [
            'cluster'       => ['集群控制', 'cluster.php'],
        ],
        '配置' => [
            'settings_server'   => ['服务器基础配置', 'settings_server.php'],
            'settings_scaling'  => ['扩缩容配置', 'settings_scaling.php'],
            'settings_nodes'    => ['节点配置', 'settings_nodes.php'],
        ],
        '更新' => [
            'gray_update'      => ['灰度更新', 'gray_update.php'],
            'script_reload'    => ['脚本热更', 'script_reload.php'],
        ],
        '恢复记录' => [
            'crashes'           => ['进程宕机', 'crashes.php'],
            'actor_recovery'    => ['Actor 恢复', 'actor_recovery.php'],
        ],
        '工具' => [
            'gm'            => ['GM 控制台', 'gm.php'],
        ],
        '技术文档' => [
            'docs_client'     => ['客户端架构', 'docs_client.php'],
            'docs_server'     => ['服务端架构', 'docs_server.php'],
            'docs_lua_hotfix' => ['Lua 热更', 'docs_lua_hotfix.php'],
            'docs_ci_cd'      => ['CI/CD 流水线', 'docs_ci_cd.php'],
        ],
    ];
}

// 导航项 → Tabler 图标名 (https://tabler.io/icons)
function nav_icon(string $key): string
{
    static $map = [
        'nodes'           => 'ti-server',
        'accounts'        => 'ti-users',
        'db_accounts'     => 'ti-database',
        'account_control' => 'ti-user-plus',
        'trace'           => 'ti-activity',
        'actors'          => 'ti-cube',
        'battles'         => 'ti-swords',
        'docs_frame_sync' => 'ti-route',
        'server_logs'     => 'ti-file-text',
        'cluster'         => 'ti-layout-dashboard',
        'settings_server' => 'ti-settings',
        'settings_scaling'=> 'ti-adjustments-horizontal',
        'settings_nodes'  => 'ti-cpu',
        'gray_update'     => 'ti-package',
        'script_reload'   => 'ti-file-code',
        'crashes'         => 'ti-alert-triangle',
        'actor_recovery'  => 'ti-heartbeat',
        'gm'              => 'ti-terminal-2',
        'docs_client'     => 'ti-device-gamepad-2',
        'docs_server'     => 'ti-server-2',
        'docs_lua_hotfix' => 'ti-refresh',
        'docs_ci_cd'      => 'ti-git-branch',
    ];
    return $map[$key] ?? 'ti-circle';
}

function layout_header(string $title, string $active = ''): void
{
    if (is_partial()) return;   // 局部刷新: 只回数据区, 不重复输出布局 chrome
    $brand = 'RealServer 控制台';
    $user  = esc($_SESSION['user'] ?? '');
    $role  = esc($_SESSION['role'] ?? '');
    echo '<!DOCTYPE html><html lang="zh-CN" data-bs-theme="dark"><head><meta charset="UTF-8">'
       . '<meta name="viewport" content="width=device-width, initial-scale=1.0">'
       . '<title>' . esc($brand . ' — ' . $title) . '</title>'
       . '<link rel="stylesheet" href="' . TABLER_CSS . '">'
       . '<link rel="stylesheet" href="' . TABLER_ICON . '">'
       . '<link rel="stylesheet" href="style.css"></head><body>'
       . '<div class="page">'
       // ---- 侧边栏 ----
       . '<aside class="navbar navbar-vertical navbar-expand-lg" data-bs-theme="dark">'
       . '<div class="container-fluid">'
       . '<button class="navbar-toggler" type="button" data-bs-toggle="collapse" data-bs-target="#sidebar-menu">'
       . '<span class="navbar-toggler-icon"></span></button>'
       . '<h1 class="navbar-brand navbar-brand-autodark">'
       . '<a href="nodes.php" class="navbar-brand-link"><span class="brand-dot"></span> ' . esc($brand) . '</a></h1>'
       . '<div class="collapse navbar-collapse" id="sidebar-menu">'
       . '<ul class="navbar-nav pt-lg-3">';
    $gi = 0;
    foreach (nav_groups() as $group => $items) {
        $groupId     = 'nav-group-' . $gi;
        $isActiveGrp = array_key_exists($active, $items);   // 当前大类 = 含激活项的分组
        $show        = $isActiveGrp ? ' show' : '';
        $expanded    = $isActiveGrp ? 'true' : 'false';
        $collapsed   = $isActiveGrp ? '' : ' collapsed';
        echo '<li class="nav-item">'
           . '<a class="nav-link nav-group-toggle' . $collapsed . '" data-bs-toggle="collapse" '
           . 'data-bs-target="#' . $groupId . '" href="#' . $groupId . '" role="button" aria-expanded="' . $expanded . '">'
           . '<span class="nav-group-title">' . esc($group) . '</span>'
           . '<span class="nav-group-chevron"><i class="ti ti-chevron-down"></i></span>'
           . '</a>'
           . '<div class="collapse' . $show . '" id="' . $groupId . '">'
           . '<ul class="nav flex-column nav-group-items">';
        foreach ($items as $key => $item) {
            $cls = ($key === $active) ? 'nav-link active' : 'nav-link';
            echo '<li class="nav-item"><a class="' . $cls . '" href="' . esc($item[1]) . '">'
               . '<span class="nav-link-icon d-md-none d-lg-inline-block"><i class="ti ' . nav_icon($key) . '"></i></span>'
               . '<span class="nav-link-title">' . esc($item[0]) . '</span></a></li>';
        }
        echo '</ul></div></li>';
        $gi++;
    }
    echo '</ul></div></div></aside>'
       // ---- 主区域 ----
       . '<div class="page-wrapper">'
       . '<header class="navbar navbar-expand-md d-print-none" data-bs-theme="dark">'
       . '<div class="container-xl">'
       . '<button class="navbar-toggler" type="button" data-bs-toggle="collapse" data-bs-target="#navbar-menu">'
       . '<span class="navbar-toggler-icon"></span></button>'
       . '<h1 class="navbar-title">' . esc($title) . '</h1>'
       . '<div class="navbar-nav flex-row order-md-last ms-auto">'
       . '<div class="nav-item dropdown">'
       . '<a href="#" class="nav-link d-flex lh-1 text-reset p-0" data-bs-toggle="dropdown" aria-expanded="false">'
       . '<span class="avatar avatar-sm" style="background-image: none">'
       . '<i class="ti ti-user"></i></span>'
       . '<div class="d-none d-xl-block ps-2"><div>' . $user . '</div>'
       . '<div class="mt-1 small text-secondary">' . $role . '</div></div></a>'
       . '<div class="dropdown-menu dropdown-menu-end dropdown-menu-arrow">'
       . '<a class="dropdown-item" href="logout.php"><i class="ti ti-logout me-1"></i>退出登录</a>'
       . '</div></div></div></div></header>'
       . '<div class="page-body"><div class="container-xl">';
}

function layout_footer(): void
{
    if (is_partial()) return;
    echo '</div></div></div></div>'
       . '<script src="' . TABLER_JS . '" defer></script>'
       . '</body></html>';
}

// ---------- 局部刷新 (只刷新数据区, 不整页重载) ----------
function is_partial(): bool
{
    return ($_GET['partial'] ?? '') === '1';
}

// 数据区起止: 普通模式用 <div id="live-region"> 包裹; ?partial=1 时不输出包裹标签,
// 整段内容即被 AJAX 拉取替换的数据区。配合 autorefresh() 使用。
function live_region_start(): void
{
    if (!is_partial()) echo '<div id="live-region">';
}

function live_region_end(): void
{
    if (!is_partial()) echo '</div>';
}

/**
 * 页面自动刷新控件 (每 N 秒只刷新 #live-region 数据区, 可暂停; 不再整页 reload)。
 * 用于实时监控页 (节点/账户/Actor/战斗/恢复记录), 让进程/Actor 的离线上线即时反映。
 * 页面在 autorefresh() 之后用 live_region_start()/live_region_end() 包裹数据区,
 * 控件与倒计时留在数据区外, 不会被替换。刷新时 fetch 当前 URL + ?partial=1 拿纯数据 HTML。
 * 传入 $manualLabel 可额外渲染一个「立即刷新」按钮 (同样只做局部刷新)。
 */
function autorefresh(int $seconds = 1, string $manualLabel = ''): void
{
    if (is_partial()) return;   // 局部刷新响应里不含控件/脚本
    $secs = max(1, $seconds);
    $manualBtn = $manualLabel !== ''
        ? '<button class="btn btn-sm" id="manual-refresh">' . esc($manualLabel) . '</button>'
        : '';
    echo '<div class="d-flex align-items-center gap-3 mb-3">'
       . '<label class="form-check form-switch mb-0">'
       . '<input class="form-check-input" type="checkbox" id="autoRefresh" checked>'
       . '<span class="form-check-label">自动刷新（每 ' . $secs . ' 秒）</span>'
       . '</label>'
       . '<span class="dim"><i class="ti ti-refresh"></i> <span id="refresh-countdown">' . $secs . 's</span></span>'
       . $manualBtn
       . '</div>'
       . '<script>(function(){var secs=' . $secs . ';var timer=null;'
       . 'function schedule(){timer=setTimeout(tick,1000);}'
       . 'function tick(){var box=document.getElementById("autoRefresh");'
       . 'var cd=document.getElementById("refresh-countdown");'
       . 'if(box&&!box.checked){if(cd)cd.textContent="已暂停";schedule();return;}'
       . 'if(secs<=0){refresh();secs=' . $secs . ';}'
       . 'if(cd)cd.textContent=secs+"s";secs--;schedule();}'
       . 'function refresh(){var region=document.getElementById("live-region");'
       . 'if(!region)return;'
       . 'var u=new URL(location.href);u.searchParams.set("partial","1");'
       . 'fetch(u).then(function(r){return r.text();}).then(function(html){'
       . 'if(html.indexOf("<html")!==-1||html.indexOf("<!DOCTYPE")!==-1){location.reload();return;}'
       . 'region.innerHTML=html;}).catch(function(){});}'
       . 'var mb=document.getElementById("manual-refresh");'
       . 'if(mb)mb.addEventListener("click",function(){refresh();secs=' . $secs . ';});'
       . 'schedule();})();</script>';
}
