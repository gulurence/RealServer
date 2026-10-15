<?php
// =============================================================================
// WebPlatform 连接配置 — 测试环境占位, 生产勿用真实凭证
// =============================================================================

// Redis Cluster — 节点状态/账户/追踪数据源 (RealServer 迁移后写入集群 7000-7005)
// 用 RedisCluster 客户端 (phpredis), 种子指向宿主机映射的 cluster 节点 host.docker.internal:7000
const REDIS_SEEDS = ['127.0.0.1:7000'];

// RealServer 服务器进程日志目录 — bin/windows/start_cluster.ps1 里 stdout/stderr 重定向
// 到 $log = <binDir>/logs, 各进程写 {gate,login,global,game}-01.out/.err, 另有 spdlog 滚动
// 日志 normal.log / normal.N.log。www 相对此路径回退 3 级到仓库根再进 bin/windows/logs。
const REALSERVER_LOG_DIR = __DIR__ . '/../../../bin/windows/logs';

// MySQL — 登录账号来源
// 指向宿主机共享 MySQL (Mysql 栈 mysql 3306), 与 RealServer 同实例
const DB_HOST = '127.0.0.1';
const DB_PORT = 3306;
const DB_NAME = 'realserver';
const DB_USER = 'root';
const DB_PASS = 'rootpass';

// 兜底管理员 (MySQL 不可用时的登录, 与 init SQL 一致)
const FALLBACK_ADMIN_USER = 'admin';
const FALLBACK_ADMIN_PASS = 'admin';

// CI/CD 构建登记令牌 — Jenkins build_ci.ps1 → api/build.php 登记构建用的共享口令
// (dev 占位, 生产请换成真随机值并走独立鉴权)
const DEPLOY_TOKEN = 'REALSERVER_DEPLOY_TOKEN';

// =============================================================================
// 游戏账户会话 (供 TestClient 走 HTTP 登录 → 拿 Session → 连 Gate 的链路)
// 数据键约定与 script/common/account.lua 一致:
//   account:{id}          → 账号信息 (Redis string, 热数据)
//   account:{id}:state    → 账户状态机 (0=Created 1=LoggedIn 2=RoleListed 3=RoleSelected 4=InGame)
//   session:{token}       → 账号 id (Redis string, TTL 过期)
// =============================================================================
const SESSION_TTL = 3600;   // 会话有效期 (秒)

function game_account_key($id): string
{
    return 'account:' . $id;
}

function game_account_state_key($id): string
{
    return 'account:' . $id . ':state';
}

function session_key($token): string
{
    return 'session:' . $token;
}

function make_token(): string
{
    return bin2hex(random_bytes(16));   // 32 字符 hex 令牌
}

/**
 * 创建游戏账户（注册）。幂等：同名账户已存在时返回已有 account_id。
 * 写入 MySQL account 表 (持久化) + Redis account:{id} / account:{id}:state (热数据)，
 * 与 api/login.php / script/common/account.lua 的键约定一致。
 *
 * @return array{ok:bool, account_id:int, error:?string}
 */
function register_account(string $username, string $password): array
{
    if ($username === '' || $password === '') {
        return ['ok' => false, 'account_id' => 0, 'error' => 'missing_fields'];
    }
    $pdo = db();
    if ($pdo === null) {
        return ['ok' => false, 'account_id' => 0, 'error' => 'db_unavailable'];
    }

    // 幂等：同名账户已存在直接返回已有 id
    $stmt = $pdo->prepare('SELECT id FROM account WHERE username = ?');
    $stmt->execute([$username]);
    $row = $stmt->fetch(PDO::FETCH_ASSOC);
    if ($row) {
        return ['ok' => true, 'account_id' => (int)$row['id'], 'error' => null];
    }

    $stmt = $pdo->prepare('INSERT INTO account (username, password, state) VALUES (?, ?, 0)');
    $stmt->execute([$username, password_hash($password, PASSWORD_BCRYPT)]);
    $accountId = (int)$pdo->lastInsertId();

    $r = redis();
    if ($r !== null) {
        $info = json_encode(['account_id' => $accountId, 'username' => $username, 'state' => 0], JSON_UNESCAPED_UNICODE);
        $r->set(game_account_key($accountId), $info);
        $r->set(game_account_state_key($accountId), '0');
    }
    return ['ok' => true, 'account_id' => $accountId, 'error' => null];
}

/**
 * 返回 PDO 连接; MySQL 不可用时返回 null (触发兜底登录)
 */
function db(): ?PDO
{
    static $pdo = null;
    if ($pdo === null) {
        try {
            $pdo = new PDO(
                'mysql:host=' . DB_HOST . ';port=' . DB_PORT . ';dbname=' . DB_NAME . ';charset=utf8mb4',
                DB_USER,
                DB_PASS,
                [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION, PDO::ATTR_TIMEOUT => 3]
            );
        } catch (PDOException $e) {
            $pdo = false;
        }
    }
    return $pdo ?: null;
}

/**
 * 返回 Redis Cluster 连接 (RedisCluster); 不可用时返回 null
 */
function redis(): ?RedisCluster
{
    static $r = null;
    if ($r === null) {
        if (!class_exists('RedisCluster')) {
            $r = false;
            return null;
        }
        try {
            $r = new RedisCluster(null, REDIS_SEEDS, 1.0, 1.0);
        } catch (Throwable $e) {
            $r = false;
        }
    }
    return $r ?: null;
}

/**
 * 遍历整个集群的匹配键 (对每个 master 节点各 SCAN 一遍, 避免 KEYS 阻塞)
 * @return list<string>
 */
function scan_cluster(?RedisCluster $r, string $pattern, int $count = 200): array
{
    $keys = [];
    if ($r === null) {
        return $keys;
    }
    try {
        foreach ($r->_masters() as $master) {
            $node = $master[0] . ':' . $master[1];
            $it = null;
            while (($batch = $r->scan($it, $node, $pattern, $count)) !== false) {
                foreach ($batch as $k) {
                    $keys[] = (string)$k;
                }
                if ($it === 0) {
                    break;
                }
            }
        }
    } catch (Throwable $e) {
        // 集群不可用时返回空
    }
    return $keys;
}

// =============================================================================
// 运行时配置 / 期望状态 / 机器 / 告警 — 数据访问 helpers (P4)
//
// 键约定与 design 文档 §4.4 / §5 一致:
//   cluster:config:cluster          全局
//   cluster:config:process:{type}   每进程类型
//   cluster:config:node:{machineId} 每机器
//   cluster:desired:{type}          Hash {count, mode}
//   cluster:node:{machineId}        Hash (机器注册)
//   cluster:node:all                Set
//   cluster:alert                   list
// =============================================================================
const NODE_ALL_KEY = 'cluster:node:all';
const ALERT_KEY   = 'cluster:alert';

function config_cluster_key(): string  { return 'cluster:config:cluster'; }
function config_game_key(): string      { return 'cluster:config:game'; }
function config_actor_state_ttl_key(): string { return 'cluster:config:actor-state-ttl'; }
function config_process_key(int $type): string { return 'cluster:config:process:' . $type; }
function config_node_key(int $machineId): string { return 'cluster:config:node:' . $machineId; }
function desired_key(int $type): string  { return 'cluster:desired:' . $type; }
function machine_node_key(int $machineId): string { return 'cluster:node:' . $machineId; }

/** 读一个 Hash (RedisCluster 不可用/无键时返回空数组) */
function redis_hash_get(?RedisCluster $r, string $key): array
{
    if ($r === null) return [];
    try {
        $h = $r->hgetall($key);
        return is_array($h) ? $h : [];
    } catch (Throwable $e) {
        return [];
    }
}

/** 写一个 Hash 若干字段 (跳过 null/空串, 用于表单回填) */
function redis_hash_set(?RedisCluster $r, string $key, array $fields): bool
{
    if ($r === null || empty($fields)) return false;
    try {
        foreach ($fields as $f => $v) {
            if ($v === null || $v === '') continue;
            $r->hset($key, (string)$f, (string)$v);
        }
        return true;
    } catch (Throwable $e) {
        return false;
    }
}

/** 拉取所有机器 (machineId => Hash) */
function fetch_machines(?RedisCluster $r): array
{
    $out = [];
    if ($r === null) return $out;
    try {
        $members = $r->smembers(NODE_ALL_KEY);
        if (is_array($members)) {
            foreach ($members as $id) {
                $h = $r->hgetall(machine_node_key((int)$id));
                if (is_array($h) && !empty($h)) {
                    $h['machine_id'] = (string)$id;
                    $out[(int)$id] = $h;
                }
            }
        }
    } catch (Throwable $e) {}
    ksort($out);
    return $out;
}

/** 拉取告警事件 (最新在前) */
function fetch_alerts(?RedisCluster $r, int $limit = 50): array
{
    if ($r === null) return [];
    try {
        $raw = $r->lrange(ALERT_KEY, 0, $limit - 1);
        if (!is_array($raw)) return [];
        return array_reverse($raw);   // RPUSH 追加 → 索引 0 最旧, 反转后最新在前
    } catch (Throwable $e) {
        return [];
    }
}

/** 表单整数/字符串取值 */
function post_int(string $field, int $default = 0): int
{
    return (int)($_POST[$field] ?? $default);
}
function post_str(string $field, string $default = ''): string
{
    return trim((string)($_POST[$field] ?? $default));
}
