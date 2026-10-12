<?php
// =============================================================================
// 链路追踪 — 拓扑可视化 (PPT 风格)
//
// 布局: 左侧固定拓扑图 (节点/连线一次绘制, 5 秒内不重排) + 右侧独立消息列表控件。
// 刷新: 5 秒局部脏刷新 (AJAX, 只更新连线颜色 + 选中链路消息列表, 不整页刷新)。
// 连线: 每条链路只有一根固定粗细连线, 颜色按峰值延迟 (绿→黄→红);
//       点击选中后高亮发光, 其余连线变暗, 右侧列表同步展示该链路慢消息。
//
// 配色 (EnvRoot/WebPlatform/src/链路追踪模板.png):
//   背景 #0A141D · 节点白底 #FFFEFC · 节点蓝边 #466DD6 · 连线青 #67E2DB
// =============================================================================
require_once __DIR__ . '/_layout.php';

// ---------------------------------------------------------------------------
// msg_id → 协议名称 (镜像 engine/xRpc/RpcFrame.h 的 ERpcMethodId 与 source/proto-def/msg/*.proto 的 MsgId 枚举)
//   小 ID (< 0x10000): RPC 方法 ID (ERpcMethodId, CTraceManager 上报的 RPC 协议延迟)
//   大 ID: (BigType<<16) | SmallType, BigType = ActorType(AT_*), SmallType = 各模块 MsgId 枚举
// ---------------------------------------------------------------------------
const RPC_MSG_NAMES = [
    1 => 'RPC_METHOD_CREATE_ACTOR',
    2 => 'RPC_METHOD_FIND_ACTOR',
    3 => 'RPC_METHOD_UNINSTALL_ACTOR',
    4 => 'RPC_METHOD_LOGIN',
    5 => 'RPC_METHOD_FORWARD_ACTOR',
    6 => 'RPC_METHOD_PUSH',
];

// 各模块消息名 (镜像 source/proto-def/msg/*.proto 的 MsgId 枚举成员)
const MODULE_MSG_NAMES = [
    101 => [   // login.proto LoginMsgId
        1 => 'LOGIN_MSG_LOGIN_REQ', 2 => 'LOGIN_MSG_LOGIN_RSP',
        3 => 'LOGIN_MSG_SELECT_ROLE_REQ', 4 => 'LOGIN_MSG_SELECT_ROLE_RSP',
    ],
    102 => [   // friend.proto FriendMsgId
        1 => 'FRIEND_ADD_REQ', 2 => 'FRIEND_ADD_RSP', 3 => 'FRIEND_ADD_APPLY_NTF',
        4 => 'FRIEND_ADD_ACCEPT_REQ', 5 => 'FRIEND_ADD_ACCEPT_RSP',
        6 => 'FRIEND_LIST_REQ', 7 => 'FRIEND_LIST_RSP',
        8 => 'FRIEND_DEL_REQ', 9 => 'FRIEND_DEL_RSP',
        10 => 'FRIEND_ONLINE_NTF', 11 => 'FRIEND_INVITE_REQ', 12 => 'FRIEND_INVITE_NTF',
    ],
    103 => [   // room.proto RoomMsgId
        1 => 'ROOM_CREATE_REQ', 2 => 'ROOM_CREATE_RSP',
        3 => 'ROOM_LIST_REQ', 4 => 'ROOM_LIST_RSP',
        5 => 'ROOM_JOIN_REQ', 6 => 'ROOM_JOIN_RSP',
        7 => 'ROOM_LEAVE_REQ', 8 => 'ROOM_LEAVE_RSP',
        9 => 'ROOM_READY_REQ', 10 => 'ROOM_READY_RSP',
        11 => 'ROOM_KICK_REQ', 12 => 'ROOM_KICK_RSP',
        13 => 'ROOM_TRANSFER_REQ', 14 => 'ROOM_START_REQ', 15 => 'ROOM_START_RSP',
        16 => 'ROOM_INFO_NTF', 17 => 'ROOM_MEMBER_NTF',
    ],
    104 => [   // chat.proto ChatMsgId
        1 => 'CHAT_SEND_REQ', 2 => 'CHAT_SEND_RSP', 3 => 'CHAT_NTF',
        4 => 'CHAT_HISTORY_REQ', 5 => 'CHAT_HISTORY_RSP',
    ],
    201 => [   // player.proto PlayerMsgId
        1 => 'PLAYER_MSG_ENTER_REQ', 2 => 'PLAYER_MSG_ENTER_RSP',
        3 => 'PLAYER_MSG_LEAVE_REQ', 4 => 'PLAYER_MSG_LEAVE_RSP',
    ],
    203 => [   // battle.proto BattleMsgId
        1 => 'BATTLE_ENTER_REQ', 2 => 'BATTLE_ENTER_RSP', 3 => 'BATTLE_START_NTF',
        4 => 'BATTLE_INPUT_REQ', 5 => 'BATTLE_FRAME_NTF', 6 => 'BATTLE_SNAPSHOT_NTF',
        7 => 'BATTLE_END_NTF', 8 => 'BATTLE_LEAVE_REQ', 9 => 'BATTLE_LEAVE_RSP',
        10 => 'BATTLE_HASH_NTF',
    ],
    204 => [   // spectator.proto SpectatorMsgId
        1 => 'SPECTATE_ENTER_REQ', 2 => 'SPECTATE_ENTER_RSP',
        3 => 'SPECTATE_LEAVE_REQ', 4 => 'SPECTATE_LEAVE_RSP',
        5 => 'SPECTATE_SNAPSHOT_NTF', 6 => 'SPECTATE_FRAME_NTF', 7 => 'SPECTATE_END_NTF',
    ],
];

function node_key(int $type, int $id): string
{
    return $type . ':' . $id;
}

/**
 * msg_id → 可读名称 (无法识别时回退为 "#<id>")
 */
function msg_name($msgId): string
{
    $msgId = (int)$msgId;
    if ($msgId < 0x10000) {
        return RPC_MSG_NAMES[$msgId] ?? ('#' . $msgId);
    }
    $big   = ($msgId >> 16) & 0xFFFF;
    $small = $msgId & 0xFFFF;
    return MODULE_MSG_NAMES[$big][$small] ?? ('#' . $msgId);
}

/**
 * 延迟 → 颜色 (绿→黄→红), 对数刻度相对阈值: 1x=绿, ~4.5x=黄, 20x=红
 */
function latency_color(float $latency, int $threshold): string
{
    $lo = max(1.0, (float)$threshold);
    $latency = max($latency, $lo);
    $t = log($latency / $lo) / log(20.0);
    $t = max(0.0, min(1.0, $t));

    $green  = [34, 197, 94];   // #22c55e
    $yellow = [234, 179, 8];   // #eab308
    $red    = [239, 68, 68];   // #ef4444

    if ($t < 0.5) {
        return rgb_lerp($green, $yellow, $t * 2.0);
    }
    return rgb_lerp($yellow, $red, ($t - 0.5) * 2.0);
}

/** RGB 线性插值 */
function rgb_lerp(array $a, array $b, float $k): string
{
    $k = max(0.0, min(1.0, $k));
    $r  = (int)round($a[0] + ($b[0] - $a[0]) * $k);
    $g  = (int)round($a[1] + ($b[1] - $a[1]) * $k);
    $bl = (int)round($a[2] + ($b[2] - $a[2]) * $k);
    return sprintf('#%02x%02x%02x', $r, $g, $bl);
}

/**
 * 拉取所有进程节点: nodeKey => [type, id, name]
 */
function fetch_nodes(?RedisCluster $r): array
{
    $nodes = [];
    if ($r === null) return $nodes;
    foreach (array_keys(PROCESS_TYPES) as $type) {
        $members = $r->smembers("cluster:process:by_type:{$type}");
        if (!is_array($members)) continue;
        foreach ($members as $id) {
            $hash = $r->hgetall("cluster:process:{$type}:{$id}");
            if (!is_array($hash) || empty($hash)) continue;
            if (!is_online($hash)) continue;   // 只显示在线节点, 离线/僵尸节点不进入拓扑
            $nodes[node_key($type, (int)$id)] = [
                'type' => $type,
                'id'   => (int)$id,
                'name' => (string)($hash['name'] ?? ("process-{$id}")),
            ];
        }
    }
    return $nodes;
}

/**
 * 拉取所有链路样本 (每链路只取 Top-N 条, 另记录总量)
 * @return array<int, array{from:string,to:string,total:int,samples:list<array>}>
 */
function fetch_links(?RedisCluster $r, int $topN): array
{
    $links = [];
    if ($r === null) return $links;

    foreach (scan_cluster($r, 'trace:link:*') as $key) {
        $key = (string)$key;
        // trace:link:{ft}:{fi}:{tt}:{ti}
        if (!preg_match('/^trace:link:(\d+):(\d+):(\d+):(\d+)$/', $key, $m)) {
            continue;
        }
        $from = node_key((int)$m[1], (int)$m[2]);
        $to   = node_key((int)$m[3], (int)$m[4]);

        $total = (int)$r->zCard($key);   // 该链路慢消息总量

        // ZREVRANGE ... WITHSCORES: 按 score(延迟) 降序, 只取前 N 条
        $raw = $r->zRevRange($key, 0, $topN - 1, true);
        $samples = [];
        if (is_array($raw)) {
            foreach ($raw as $member => $score) {
                $data = json_decode((string)$member, true);
                if (!is_array($data)) {
                    $data = ['raw' => (string)$member];
                }
                $data['latency_ms'] = (float)($data['latency_ms'] ?? $score);
                $data['name']       = msg_name($data['msg_id'] ?? 0);
                $samples[] = $data;
            }
        }

        $links[] = ['from' => $from, 'to' => $to, 'total' => $total, 'samples' => $samples];
    }
    return $links;
}

/**
 * 计算节点布局坐标 (nodeKey => [x, y])。
 *
 * 分层: 对链路做「最长路径」分层 (无环 DAG 精确; 有环时退化为 BFS 深度), 使依赖靠前的
 * 进程在左、靠后的在右; 同一层的进程垂直展开。这样「客户端→Gate→后端」这类星形扇出会
 * 把 Login/Global/Game 放到同一列垂直排列, 从根本上避免多进程同排时连线重合或穿过中间节点。
 * 层内排序: barycenter (按入边来源在上一层的位置均值) 减少连线交叉; 同级按 type,id 兜底确定性。
 */
function compute_layout(array $nodes, array $links): array
{
    $keys = array_keys($nodes);
    if (empty($keys)) return [];

    // 1) 邻接表 (仅保留两端都在节点集合内的边)
    $inAdj = [];
    $outAdj = [];
    foreach ($keys as $k) { $inAdj[$k] = []; $outAdj[$k] = []; }
    foreach ($links as $lk) {
        $f = $lk['from']; $t = $lk['to'];
        if (!isset($nodes[$f]) || !isset($nodes[$t])) continue;
        $outAdj[$f][] = $t;
        $inAdj[$t][]  = $f;
    }

    // 2) 最长路径分层 (relaxation 直到稳定; 迭代上限兜底环)
    $layer = array_fill_keys($keys, 0);
    for ($it = 0; $it < 64; $it++) {
        $changed = false;
        foreach ($keys as $k) {
            foreach ($outAdj[$k] as $t) {
                if ($layer[$t] <= $layer[$k]) { $layer[$t] = $layer[$k] + 1; $changed = true; }
            }
        }
        if (!$changed) break;
    }
    // 环兜底: 层数 ≥ 节点数说明存在环, 退化为 BFS 深度 (从入度为 0 的源出发)
    if (max($layer) >= count($keys)) {
        $layer = array_fill_keys($keys, 0);
        $q = [];
        foreach ($keys as $k) if (empty($inAdj[$k])) $q[] = $k;
        if (empty($q)) $q[] = $keys[0];
        $seen = [];
        $depth = 0;
        foreach ($q as $k) { $layer[$k] = 0; $seen[$k] = true; }
        while (!empty($q)) {
            $depth++;
            $next = [];
            foreach ($q as $k) {
                foreach ($outAdj[$k] as $t) {
                    if (!isset($seen[$t])) { $seen[$t] = true; $layer[$t] = $depth; $next[] = $t; }
                }
            }
            $q = $next;
        }
    }

    // 3) 层内排序: barycenter (入边来源在上一层行号的均值), 减少交叉; 同级按 type,id 兜底确定性
    $byLayer = [];
    foreach ($keys as $k) $byLayer[$layer[$k]][] = $k;
    ksort($byLayer);
    $row = [];           // node => 层内行号
    $ordered = [];       // layer => 有序节点列表
    foreach ($byLayer as $lv => $members) {
        if (count($members) > 1) {
            $bary = [];
            foreach ($members as $k) {
                $sum = 0.0; $cnt = 0;
                foreach ($inAdj[$k] as $src) {
                    if (isset($row[$src])) { $sum += $row[$src]; $cnt++; }
                }
                $bary[$k] = ($cnt > 0) ? ($sum / $cnt) : 1e9;   // 无入边置后
            }
            usort($members, function ($a, $b) use ($bary, $nodes) {
                $d = $bary[$a] <=> $bary[$b];
                if ($d !== 0) return $d;
                $dt = ($nodes[$a]['type'] <=> $nodes[$b]['type']);
                if ($dt !== 0) return $dt;
                return ($nodes[$a]['id'] <=> $nodes[$b]['id']);
            });
        }
        $ordered[$lv] = $members;
        foreach ($members as $i => $k) $row[$k] = $i;
    }

    // 4) 坐标: x = 层序 × 列宽, y = 层内垂直居中 (相对最满的一层, 使整图上下平衡)
    $maxRows = 1;
    foreach ($ordered as $members) $maxRows = max($maxRows, count($members));
    $colW = 260;
    $rowH = 140;
    $baseX = 120;
    $centerY = ($maxRows - 1) * $rowH / 2 + 40;
    $layout = [];
    foreach ($ordered as $lv => $members) {
        $n = count($members);
        $top = $centerY - ($n - 1) * $rowH / 2;
        foreach ($members as $i => $k) {
            $layout[$k] = [$baseX + $lv * $colW, $top + $i * $rowH];
        }
    }
    return $layout;
}

$r = redis();
$isAjax = (($_GET['ajax'] ?? '') === '1');          // 局部脏刷新数据接口
$topN  = max(1, min(500, (int)($_GET['top'] ?? 20)));   // 每条链路显示的最高延迟条数
$selectedLink = $_GET['link'] ?? null;                  // 刷新后保持选中的链路 (from:to)
if ($selectedLink !== null && !preg_match('/^\d+:\d+:\d+:\d+$/', (string)$selectedLink)) {
    $selectedLink = null;
}
$nodes = fetch_nodes($r);
$links = fetch_links($r, $topN);

// 当前阈值
$threshold = 50;
if ($r !== null) {
    $v = $r->hGet('trace:config', 'latency_threshold_ms');
    if ($v !== false && ctype_digit((string)$v)) $threshold = (int)$v;
}

// 注意: 不再把「链路端点里但不在服务发现中」的节点补回来 —— 那些是已注销/离线的
// 残留 trace 数据, 按「只显示在线节点」的约定应隐藏; 对应连线会因端点不在布局里被
// 自动跳过 (见下方 graph 组装处的 isset($layout[...]) 判断)。

// 把「客户端」虚拟节点 (0:0) 纳入布局: 只要链路引用了 0:0 就加入 (它是链路的源)。
$hasUnknown = false;
foreach ($links as $lk) {
    if ($lk['from'] === '0:0' || $lk['to'] === '0:0') { $hasUnknown = true; break; }
}
if ($hasUnknown) {
    $nodes['0:0'] = ['type' => 0, 'id' => 0, 'name' => '客户端'];
}

// 计算节点布局: 按链路依赖关系分层 (客户端→Gate→后端), 层内垂直展开, 避免连线重合/穿过节点。
$layout = compute_layout($nodes, $links);

$totalSamples = array_sum(array_map(fn($lk) => count($lk['samples']), $links));

// 组装给前端的数据
$graph = ['nodes' => [], 'links' => []];
foreach ($nodes as $k => $n) {
    [$x, $y] = $layout[$k] ?? [0, 0];
    $isUnknown = ($n['type'] === 0);
    $graph['nodes'][] = [
        'key'  => $k,
        'type' => $n['type'],
        'id'   => $n['id'],
        'name' => $n['name'],
        'x'    => $x,
        'y'    => $y,
        'w'    => $isUnknown ? 88 : 120,   // 节点宽度
        'h'    => $isUnknown ? 40 : 46,   // 节点高度
    ];
}
foreach ($links as $lk) {
    if (!isset($layout[$lk['from']]) || !isset($layout[$lk['to']])) continue;
    // 每条样本附颜色 (按自身延迟); 记录链路峰值延迟作为连线颜色
    $maxLat = 0.0;
    $samples = $lk['samples'];
    foreach ($samples as &$s) {
        $lat = (float)($s['latency_ms'] ?? 0);
        $s['color'] = latency_color($lat, $threshold);
        $maxLat = max($maxLat, $lat);
    }
    unset($s);

    // 连线只有一根, 固定粗细 (粗细不再随消息总量变化)
    $graph['links'][] = [
        'from'    => $lk['from'],
        'to'      => $lk['to'],
        'total'   => $lk['total'],
        'latency' => round($maxLat, 1),
        'color'   => latency_color($maxLat, $threshold),
        'samples' => $samples,
    ];
}
$graphJson = json_encode($graph, JSON_UNESCAPED_UNICODE);

// 计算画布包围盒: 按节点实际布局 + 宽高 + 边距, 避免固定 viewBox 裁切右侧/下方节点
$vbPad = 40;
$vbMinX = $vbMinY = PHP_INT_MAX;
$vbMaxX = $vbMaxY = PHP_INT_MIN;
foreach ($graph['nodes'] as $gn) {
    $vbMinX = min($vbMinX, $gn['x'] - $gn['w'] / 2);
    $vbMaxX = max($vbMaxX, $gn['x'] + $gn['w'] / 2);
    $vbMinY = min($vbMinY, $gn['y'] - $gn['h'] / 2);
    $vbMaxY = max($vbMaxY, $gn['y'] + $gn['h'] / 2);
}
$vbX = $vbMinX - $vbPad;
$vbY = $vbMinY - $vbPad;
$vbW = $vbMaxX - $vbMinX + 2 * $vbPad;
$vbH = $vbMaxY - $vbMinY + 2 * $vbPad;
$viewBox = "{$vbX} {$vbY} {$vbW} {$vbH}";

// 局部脏刷新接口: 只回传会变化的数据 (链路颜色 + 样本 + 汇总), 节点布局不变
if ($isAjax) {
    header('Content-Type: application/json; charset=utf-8');
    header('Cache-Control: no-store');
    echo json_encode([
        'threshold'    => $threshold,
        'topN'         => $topN,
        'nodeCount'    => count($graph['nodes']),
        'linkCount'    => count($graph['links']),
        'totalSamples' => $totalSamples,
        'links'        => $graph['links'],
    ], JSON_UNESCAPED_UNICODE);
    exit;
}
?>
<?php layout_header('链路追踪拓扑', 'trace'); ?>
    <style>
        /* ===== 链路面板 PPT 风格 (配色取自 链路追踪模板.png) ===== */
        :root {
            --t-bg: #0A141D;        /* 深海军蓝画布底 */
            --t-panel: #0B151F;     /* 面板底 */
            --t-border: #466DD6;    /* 蓝边 (节点/面板) */
            --t-link: #67E2DB;      /* 青连线 */
            --t-node-fill: #FFFEFC; /* 节点白底 */
            --t-node-text: #0A141D; /* 节点深色文字 */
            --t-sub: #5A5A64;       /* 次级灰字 */
        }
        .trace-nav a { color: var(--dim); text-decoration: none; margin-right: 16px; }
        .trace-nav a.active { color: var(--accent); }

        .topo-wrap {
            display: flex; gap: 20px; flex-wrap: wrap; align-items: stretch;
        }
        /* 左侧: 固定拓扑图 (只画一次, 不随 5s 刷新重排) */
        .topo-canvas {
            background: var(--t-bg);
            border: 1px solid #16324a;
            border-radius: 12px;
            flex: 1 1 560px; min-width: 560px;
            box-shadow: 0 8px 24px rgba(0,0,0,.35);
        }
        /* 右侧: 独立消息列表控件 */
        .topo-panel {
            flex: 1 1 380px; min-width: 340px;
            display: flex; flex-direction: column;
            background: var(--t-panel);
            border: 1px solid #16324a;
            border-radius: 12px; padding: 18px;
            box-shadow: 0 8px 24px rgba(0,0,0,.35);
        }
        .topo-panel h3 {
            margin: 0 0 4px; font-size: 15px; color: var(--text);
        }
        .topo-panel .sub { color: var(--dim); font-size: 12px; margin-bottom: 12px; }
        .topo-panel table { background: transparent; }
        .topo-panel th, .topo-panel td { border-bottom: 1px solid #1a2a3a; }
        /* 消息列表滚动区 (控件主体) */
        .msg-list {
            flex: 1 1 auto; min-height: 220px; max-height: 420px;
            overflow-y: auto; margin-top: 4px;
        }
        .msg-list .empty { color: var(--dim); font-size: 13px; padding: 24px 0; }

        /* 连线: 一根固定粗细, 选中高亮发光, 其余变暗 */
        .link-line {
            cursor: pointer; stroke-width: 5;
            transition: opacity .15s, stroke .15s;
        }
        .link-line:hover { opacity: .8; }
        .link-line.dim { opacity: .35; }
        .link-line.selected {
            stroke: #7ffcf4;
            stroke-width: 7.5;
            opacity: 1;
            filter: url(#lineGlow);
            animation: selPulse 1.3s ease-in-out infinite;
        }
        @keyframes selPulse {
            0%, 100% { stroke-opacity: 1; }
            50%      { stroke-opacity: .6; }
        }

        .badge-dir { padding: 1px 8px; border-radius: 6px; font-size: 11px; }
        .badge-dir.send { background: rgba(103,226,219,.15); color: #67E2DB; }
        .badge-dir.recv { background: rgba(70,109,214,.18); color: #7ea0e8; }

        /* 顶部 Top-N 设置条 */
        .top-control {
            display: inline-flex; align-items: center; gap: 8px; margin-left: 18px;
            color: var(--dim); font-size: 13px;
        }
        .top-control input {
            width: 64px; padding: 4px 8px; border-radius: 6px;
            border: 1px solid #16324a; background: var(--t-panel); color: var(--text);
            font-size: 13px;
        }
        .top-control button {
            padding: 4px 12px; border: 0; border-radius: 6px;
            background: var(--t-border); color: #fff; font-size: 13px; cursor: pointer;
        }
        .legend { color: var(--dim); font-size: 12px; margin-top: 10px; }
        .legend .dot { display: inline-block; width: 16px; height: 4px; border-radius: 2px; vertical-align: middle; margin-right: 6px; }
    </style>
        <div class="summary">
            节点：<b id="sum-nodes"><?= count($graph['nodes']) ?></b>
            · 链路：<b id="sum-links"><?= count($graph['links']) ?></b>
            · 慢消息：<b id="sum-samples"><?= $totalSamples ?></b>
            · 阈值：<b id="sum-threshold"><?= $threshold ?> ms</b>
            <form method="get" action="trace_topology.php" class="top-control">
                显示最高延迟 Top
                <input class="form-control" type="number" name="top" min="1" max="500" value="<?= $topN ?>"> 条
                <input type="hidden" name="link" id="link-input" value="<?= esc($selectedLink ?? '') ?>">
                <button class="btn btn-primary" type="submit">应用</button>
            </form>
            <span class="dim">（每 5 秒局部刷新，不重载页面；点击连线高亮并查看该链路 Top-<?= $topN ?> 慢消息）</span>
        </div>

        <nav class="trace-nav">
            <a href="trace.php">阈值配置</a>
            <a href="trace_topology.php" class="active">拓扑图</a>
        </nav>

        <?php if ($r === null): ?>
            <div class="empty">⚠️ 无法连接 Redis，请确认 Redis 已启动。</div>
        <?php elseif (count($graph['nodes']) === 0): ?>
            <div class="empty">暂无进程节点 —— 启动 RealServer 后，这里会显示节点拓扑；慢消息需实际流量触发（延迟 &gt; <?= $threshold ?>ms）。</div>
        <?php else: ?>
            <div class="topo-wrap">
                <svg id="topo" class="topo-canvas" viewBox="<?= $viewBox ?>" preserveAspectRatio="xMinYMin meet">
                    <defs>
                        <marker id="arrow" viewBox="0 0 10 10" refX="9" refY="5"
                                markerWidth="11" markerHeight="11" orient="auto-start-reverse">
                            <path d="M 0 1 L 9 5 L 0 9 z" fill="context-stroke"/>
                        </marker>
                        <filter id="lineGlow" filterUnits="userSpaceOnUse" x="<?= $vbX ?>" y="<?= $vbY ?>" width="<?= $vbW ?>" height="<?= $vbH ?>">
                            <feGaussianBlur stdDeviation="4" result="blur"/>
                            <feMerge>
                                <feMergeNode in="blur"/>
                                <feMergeNode in="SourceGraphic"/>
                            </feMerge>
                        </filter>
                        <filter id="nodeShadow" x="-30%" y="-40%" width="160%" height="180%">
                            <feDropShadow dx="0" dy="3" stdDeviation="4" flood-color="#000000" flood-opacity="0.4"/>
                        </filter>
                    </defs>
                </svg>
                <div class="topo-panel">
                    <h3>链路消息 <span class="dim" id="sel-link-label"></span></h3>
                    <div class="sub">按延迟降序，最多显示 Top-<?= $topN ?> 条</div>
                    <div class="legend">
                        连线颜色 = 峰值延迟：
                        <span class="dot" style="background:#22c55e;"></span>低
                        <span class="dot" style="background:#eab308;margin-left:12px;"></span>中
                        <span class="dot" style="background:#ef4444;margin-left:12px;"></span>高
                    </div>
                    <div id="sample-list" class="msg-list">
                        <div class="empty">点击左侧连线查看该链路的慢消息。</div>
                    </div>
                </div>
            </div>
        <?php endif; ?>

    <script>
    (function () {
        var graph = <?= $graphJson ?>;
        var selectedLink = <?= json_encode($selectedLink) ?>;
        var topN = <?= $topN ?>;
        var svg = document.getElementById('topo');
        if (!svg || !graph) return;

        var NS = 'http://www.w3.org/2000/svg';
        var LINE_W = 5;     // 固定连线宽度
        var SEL_W  = 7.5;   // 选中连线宽度 (CSS 内同名值, 保持一致)
        var nodePos = {};
        graph.nodes.forEach(function (n) { nodePos[n.key] = n; });

        var linksByKey = {};   // "from:to" -> { line, meta }
        var selectedKey = null;

        // 分两层绘制: 连线层在下, 节点层在上 (动态新增连线也保持在节点之下)
        var linesGroup = document.createElementNS(NS, 'g');
        var nodesGroup = document.createElementNS(NS, 'g');
        svg.appendChild(linesGroup);
        svg.appendChild(nodesGroup);

        // 沿连线方向两端各缩短到节点矩形边界 (半宽/半高求交), 水平/垂直/斜线都正确
        function edge(a, b) {
            var dx = b.x - a.x, dy = b.y - a.y;
            var len = Math.sqrt(dx * dx + dy * dy);
            if (len < 1) return { x1: a.x, y1: a.y, x2: b.x, y2: b.y };
            var ux = dx / len, uy = dy / len;
            function trim(hw, hh) {
                var tx = Math.abs(ux) < 1e-6 ? Infinity : hw / Math.abs(ux);
                var ty = Math.abs(uy) < 1e-6 ? Infinity : hh / Math.abs(uy);
                return Math.min(tx, ty);
            }
            var da = trim(a.w / 2, a.h / 2) + 4;   // +4 给箭头留一点空隙
            var db = trim(b.w / 2, b.h / 2) + 6;
            return { x1: a.x + ux * da, y1: a.y + uy * da,
                     x2: b.x - ux * db, y2: b.y - uy * db };
        }

        function makeLine(lk) {
            var a = nodePos[lk.from], b = nodePos[lk.to];
            if (!a || !b) return null;
            var e = edge(a, b);
            var line = document.createElementNS(NS, 'line');
            line.setAttribute('x1', e.x1); line.setAttribute('y1', e.y1);
            line.setAttribute('x2', e.x2); line.setAttribute('y2', e.y2);
            line.setAttribute('class', 'link-line');
            line.setAttribute('stroke', lk.color || '#67E2DB');
            line.setAttribute('stroke-width', LINE_W);
            line.setAttribute('stroke-linecap', 'round');
            line.setAttribute('marker-end', 'url(#arrow)');
            line.addEventListener('click', function () { selectLink(lk.from, lk.to); });
            linesGroup.appendChild(line);
            return line;
        }

        // 初始绘制连线 (固定一根, 粗细不随消息量变化)
        graph.links.forEach(function (lk) {
            var line = makeLine(lk);
            if (line) linksByKey[lk.from + ':' + lk.to] = { line: line, meta: lk };
        });

        // 绘制节点 (固定, 只画一次, 5 秒刷新不重排)
        graph.nodes.forEach(function (n) {
            var g = document.createElementNS(NS, 'g');
            var r = document.createElementNS(NS, 'rect');
            var hw = n.w / 2, hh = n.h / 2;
            r.setAttribute('x', n.x - hw); r.setAttribute('y', n.y - hh);
            r.setAttribute('width', n.w); r.setAttribute('height', n.h);
            r.setAttribute('rx', 8);
            r.setAttribute('filter', 'url(#nodeShadow)');
            if (n.type === 0) {
                r.setAttribute('fill', '#1a2233');
                r.setAttribute('stroke', '#5A5A64');
                r.setAttribute('stroke-dasharray', '5 4');
            } else {
                r.setAttribute('fill', '#FFFEFC');
                r.setAttribute('stroke', '#466DD6');
            }
            r.setAttribute('stroke-width', 2.5);
            g.appendChild(r);

            var t1 = document.createElementNS(NS, 'text');
            t1.setAttribute('x', n.x); t1.setAttribute('y', n.y - 3);
            t1.setAttribute('text-anchor', 'middle');
            t1.setAttribute('font-size', 13); t1.setAttribute('font-weight', 600);
            t1.setAttribute('fill', n.type === 0 ? '#7a869c' : '#0A141D');
            t1.textContent = n.name;
            g.appendChild(t1);

            var t2 = document.createElementNS(NS, 'text');
            t2.setAttribute('x', n.x); t2.setAttribute('y', n.y + 14);
            t2.setAttribute('text-anchor', 'middle');
            t2.setAttribute('font-size', 10);
            t2.setAttribute('fill', '#5A5A64');
            t2.textContent = (n.type === 0) ? '客户端' : (PROCESS_LABEL(n.type) + '#' + n.id);
            g.appendChild(t2);

            nodesGroup.appendChild(g);
        });

        function PROCESS_LABEL(t) {
            return {100:'Gate',101:'Login',102:'Global',201:'Game'}[t] || ('T' + t);
        }

        // 根据选中态刷新全部连线的颜色/粗细/明暗
        function refreshLineVisual() {
            Object.keys(linksByKey).forEach(function (k) {
                var entry = linksByKey[k];
                var isSel = (k === selectedKey);
                entry.line.classList.toggle('selected', isSel);
                entry.line.classList.toggle('dim', selectedKey !== null && !isSel);
                entry.line.setAttribute('stroke-width', isSel ? SEL_W : LINE_W);
                entry.line.setAttribute('stroke', entry.meta.color || '#67E2DB');
            });
        }

        // 右侧消息列表控件渲染 (按当前选中链路)
        function renderSamplePanel(key) {
            var label = document.getElementById('sel-link-label');
            var box = document.getElementById('sample-list');
            if (!key || !linksByKey[key]) {
                label.textContent = '';
                box.innerHTML = '<div class="empty">点击左侧连线查看该链路的慢消息。</div>';
                return;
            }
            var lk = linksByKey[key].meta;
            var fromName = (nodePos[lk.from] || {}).name || lk.from;
            var toName = (nodePos[lk.to] || {}).name || lk.to;
            label.textContent = ' (' + fromName + ' → ' + toName + ', 峰值 ' + (lk.latency || '?') + 'ms, 共 ' + lk.total + ' 条)';

            var samples = (lk.samples || []).slice().sort(function (a, b) {
                return (b.latency_ms || 0) - (a.latency_ms || 0);
            });
            if (samples.length === 0) {
                box.innerHTML = '<div class="empty">该链路暂无慢消息。</div>';
                return;
            }
            var html = '<table class="table table-vcenter"><thead><tr>' +
                '<th>延迟(ms)</th><th>名称</th><th>方向</th><th>消息ID</th><th>时间</th>' +
                '</tr></thead><tbody>';
            samples.forEach(function (s) {
                var isSend = (s.direction === 'send');
                html += '<tr>' +
                    '<td class="mono" style="color:' + (s.color || '#67E2DB') + ';">' + (s.latency_ms ?? '-') + '</td>' +
                    '<td>' + escHtml(s.name || ('#' + s.msg_id)) + '</td>' +
                    '<td><span class="badge-dir ' + (isSend ? 'send' : 'recv') + '">' +
                        (isSend ? '发出' : '接收') + '</span></td>' +
                    '<td class="mono">' + (s.msg_id ?? '-') + '</td>' +
                    '<td class="mono">' + fmtTs(s.ts) + '</td>' +
                    '</tr>';
            });
            html += '</tbody></table>';
            box.innerHTML = html;
        }

        function escHtml(s) {
            return String(s).replace(/[&<>"]/g, function (c) {
                return { '&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;' }[c];
            });
        }

        function fmtTs(ts) {
            if (!ts) return '-';
            var d = new Date(ts * 1000);
            function p(n) { return (n < 10 ? '0' : '') + n; }
            return p(d.getHours()) + ':' + p(d.getMinutes()) + ':' + p(d.getSeconds());
        }

        // 选中一条链路: 高亮 + 渲染消息列表 + 同步 URL (刷新后仍恢复选中)
        function selectLink(from, to) {
            var key = from + ':' + to;
            selectedKey = key;
            refreshLineVisual();
            renderSamplePanel(key);
            syncUrl(key);
        }

        function syncUrl(key) {
            var linkInput = document.getElementById('link-input');
            if (linkInput) linkInput.value = key || '';
            var url = 'trace_topology.php?top=' + topN + (key ? '&link=' + key : '');
            if (window.history && history.replaceState) history.replaceState(null, '', url);
        }

        // ---- 局部脏刷新 (5 秒, 不整页刷新) ----
        function applyDirty(data) {
            // 汇总数字
            if (data.threshold != null) {
                var th = document.getElementById('sum-threshold');
                if (th) th.textContent = data.threshold + ' ms';
            }
            if (data.nodeCount != null) {
                var sn = document.getElementById('sum-nodes');
                if (sn) sn.textContent = data.nodeCount;
            }
            if (data.linkCount != null) {
                var sl = document.getElementById('sum-links');
                if (sl) sl.textContent = data.linkCount;
            }
            if (data.totalSamples != null) {
                var ss = document.getElementById('sum-samples');
                if (ss) ss.textContent = data.totalSamples;
            }

            // 更新每条链路数据 (颜色/样本); 新出现的链路动态补画
            (data.links || []).forEach(function (lk) {
                var key = lk.from + ':' + lk.to;
                if (linksByKey[key]) {
                    linksByKey[key].meta = lk;
                } else {
                    var line = makeLine(lk);
                    if (line) linksByKey[key] = { line: line, meta: lk };
                }
            });

            refreshLineVisual();
            if (selectedKey) renderSamplePanel(selectedKey);   // 同步刷新选中位置
        }

        function poll() {
            fetch('trace_topology.php?ajax=1&top=' + topN)
                .then(function (r) { return r.json(); })
                .then(applyDirty)
                .catch(function () { /* 瞬时网络/解析错误忽略, 下轮重试 */ });
        }

        // 页面加载后恢复选中 (若有 ?link=)
        if (selectedLink && linksByKey[selectedLink]) {
            selectedKey = selectedLink;
            refreshLineVisual();
            renderSamplePanel(selectedKey);
        }

        setInterval(poll, 5000);
    })();
    </script>
<?php layout_footer(); ?>
