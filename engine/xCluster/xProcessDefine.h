#pragma once

#include "xBase/xDefine.h"
#include "xBase/xHmacSha256.h"
#include "xLog/xLog.h"
#include <string>
#include <map>

// =============================================================================
// 集群链接状态变化日志 — 绿色显示
// =============================================================================
#define CLUSTER_GREEN "\033[32m"
#define CLUSTER_RESET "\033[0m"

// 绿色日志宏 — 用于进程链接/发现/状态变化
#define XCLOG(fmt, ...)                                                                            \
    XLOG(CLUSTER_GREEN "[CLUSTER] " fmt CLUSTER_RESET __VA_OPT__(, ) __VA_ARGS__)
#define XCWRN(fmt, ...)                                                                            \
    XWRN(CLUSTER_GREEN "[CLUSTER] " fmt CLUSTER_RESET __VA_OPT__(, ) __VA_ARGS__)
#define XCERR(fmt, ...)                                                                            \
    XERR(CLUSTER_GREEN "[CLUSTER] " fmt CLUSTER_RESET __VA_OPT__(, ) __VA_ARGS__)
#include <vector>

// =============================================================================
// 类型系统 — 枚举定义已上移到 xBase/xTypeDefine.h (经 xDefine.h include):
//
//   EProcessType (P 前缀): 部署容器类型, 决定本进程可承载哪些 Actor 类型
//   EActorType   (A 前缀): 进程内动态负载的 Actor 类
//   EProcessState          : 进程运行状态
//
// 两者值域重叠 (102 进程层是 Global、Actor 层是 Friend; 201 进程层是 Game、Actor 层是 Player),
// 但语义不同, 靠 P/A/AT_ 前缀区分:
//   - 201 在进程层是 "Game"(游戏进程), 在 Actor 层是 "Player"(玩家 Actor)
//   - 承载关系由 GetHostedActorTypes 声明, 进程启动后用于 Actor 动态负载均衡
// =============================================================================

// 进程类型 → 可承载的 Actor 类型列表 (配置表, 用于启动后 Actor 动态负载)
inline std::vector<EActorType> GetHostedActorTypes(EProcessType t)
{
    switch (t)
    {
    case EProcessType_PGate:
        return {EActorType_AGate};
    case EProcessType_PLogin:
        return {EActorType_ALogin};
    case EProcessType_PGlobal:
        return {EActorType_AFriend, EActorType_AChat, EActorType_AConfig};
    case EProcessType_PGame:
        return {EActorType_APlayer, EActorType_ARoom};
    case EProcessType_PBattle:
        return {EActorType_ABattle, EActorType_ASpectator};
    default:
        return {};
    }
}

// Actor 类型 → 承载它的进程类型 (反向映射)
inline EProcessType ActorTypeToProcessType(EActorType actorType)
{
    switch (actorType)
    {
    case EActorType_AGate:
        return EProcessType_PGate;
    case EActorType_ALogin:
        return EProcessType_PLogin;
    case EActorType_AFriend:
        return EProcessType_PGlobal;
    case EActorType_ARoom:
        return EProcessType_PGame;
    case EActorType_AChat:
        return EProcessType_PGlobal;
    case EActorType_AConfig:
        return EProcessType_PGlobal;
    case EActorType_APlayer:
        return EProcessType_PGame;
    case EActorType_ABattle:
        return EProcessType_PBattle;
    case EActorType_ASpectator:
        return EProcessType_PBattle;
    default:
        return EProcessType_None;
    }
}

// Actor 类型 → 脚本目录名 (用于按类型加载 script/{platform}/<dir>/init.lua)
inline const char *ActorTypeDirName(EActorType actorType)
{
    switch (actorType)
    {
    case EActorType_AGate:
        return "gate";
    case EActorType_ALogin:
        return "login";
    case EActorType_AFriend:
        return "friend";
    case EActorType_ARoom:
        return "room";
    case EActorType_AChat:
        return "chat";
    case EActorType_AConfig:
        return "config";
    case EActorType_APlayer:
        return "player";
    case EActorType_ABattle:
        return "battle";
    case EActorType_ASpectator:
        return "spectator";
    default:
        return nullptr;
    }
}

// 脚本目录名 → Actor 类型 (ActorTypeDirName 的逆映射, 用于热更时判断文件是否属本进程托管)
inline EActorType ActorTypeFromDirName(const std::string &dir)
{
    if (dir == "gate") return EActorType_AGate;
    if (dir == "login") return EActorType_ALogin;
    if (dir == "friend") return EActorType_AFriend;
    if (dir == "room") return EActorType_ARoom;
    if (dir == "chat") return EActorType_AChat;
    if (dir == "config") return EActorType_AConfig;
    if (dir == "player") return EActorType_APlayer;
    if (dir == "battle") return EActorType_ABattle;
    if (dir == "spectator") return EActorType_ASpectator;
    return EActorType_None;
}

inline const char *ProcessTypeName(EProcessType t)
{
    switch (t)
    {
    case EProcessType_PGate:
        return "Gate";
    case EProcessType_PLogin:
        return "Login";
    case EProcessType_PGlobal:
        return "Global";
    case EProcessType_PGame:
        return "Game";
    case EProcessType_PBattle:
        return "Battle";
    default:
        return "Unknown";
    }
}

// 进程类型值域校验 — 是否为已定义的合法进程类型
inline bool IsValidProcessType(EProcessType t)
{
    switch (t)
    {
    case EProcessType_PGate:
    case EProcessType_PLogin:
    case EProcessType_PGlobal:
    case EProcessType_PGame:
    case EProcessType_PBattle:
        return true;
    default:
        return false;
    }
}

// 进程运行状态 — 枚举定义在 xBase/xTypeDefine.h
inline const char *ProcessStateName(EProcessState s)
{
    switch (s)
    {
    case EProcessState_Starting:
        return "Starting";
    case EProcessState_Init:
        return "Init";
    case EProcessState_Running:
        return "Running";
    case EProcessState_Paused:
        return "Paused";
    case EProcessState_Stopping:
        return "Stopping";
    case EProcessState_Shutdown:
        return "Shutdown";
    default:
        return "Unknown";
    }
}

inline bool IsProcessRunning(EProcessState s)
{
    return s == EProcessState_Running;
}

// =============================================================================
// 资源感知负载均衡阈值 (P1 硬编码默认; P4 迁到 Redis cluster:config:process:{type})
// =============================================================================
constexpr int32 kCpuFullPercentDefault = 90; // 进程 CPU 满载阈值 (%): 超此不再分配新 Actor
constexpr int32 kMemFullPercentDefault = 90;    // 进程内存满载阈值 (%)
constexpr int32 kActorMaxPerProcessDefault = 0; // 单进程 Actor 数上限, 0 = 不限

// 进程二进制版本 — 由 git 派生 (xVersion.h, CMake configure 生成: 短哈希+提交时间)。
// 进程级灰度更新时据此比对是否需滚动升级; 每次提交自动变化, 无需手改。
// 游戏内容版本 (content_version) 单独维护, 由灰度桶命中动态切换。
#include "xVersion.h"
constexpr const char *kProcessVersion = GIT_VERSION;

// =============================================================================
// 进程身份信息
// =============================================================================
struct ProcessInfo
{
    EProcessType type = EProcessType_None;
    int64 processId = 0;            // Redis 自增生成的唯一进程 ID
    std::string processName;        // 进程名称 (e.g. "gate-01")
    std::string host = "127.0.0.1"; // 绑定地址 (0.0.0.0 = 全接口)
    std::string ip = "127.0.0.1";   // 本机实际 IPv4 (注册/发现用)
    uint16 rpcPort = 50551;         // 本进程 RPC 端口 (初始化时分配)
    uint16 listenPort = 0;          // 业务 TCP 监听端口 (Gate 用, 记录进 local.info)
    std::string authKey;            // 验证 Key (进程 ID 派生)
    int32 loadScore = 0;            // 负载分数 (actor 实例数)
    uint32 machineId = 0;           // 所属物理机 (NodeManager 分配; 单机=0)
    int32 cpuPercent = 0;           // 自进程 CPU 占比 (0-100, 心跳采样)
    int64 memRssMb = 0;             // 自进程常驻内存 RSS (MB)
    int32 memPercent = 0;           // 自进程内存占本机总内存百分比 (0-100)
    std::string version = "1.0.0";  // 进程二进制版本 (编译期常量)
    std::string contentVersion = "1.0.0"; // 游戏内容版本 (灰度更新动态切换)
    EProcessState state = EProcessState_Starting;
    int64 lastHeartbeat = 0; // Unix 秒时间戳
    bool draining = false;   // 排空中: 不再接受新 Actor 分配 (Battle 进程级排空用, 见 gray_roll.ps1)
    int64 osPid = 0;         // 本进程 OS PID (供编排脚本把 OS pid 映射到 processId 轮询排空进度)

    // Redis Hash 序列化 key
    std::string RedisKey() const
    {
        char buf[128] = {0};
        snprintf(buf, sizeof(buf), "cluster:process:%d:%lld", (int32)type, (int64)processId);
        return buf;
    }
};

// =============================================================================
// 各类型进程的基础 RPC 端口 (类型基础端口 + 递增分配)
//
// 每个类型预留 50 个端口区间, 避免同机多实例与相邻类型冲突:
//   Gate 50551-50600 / Login 50601-50650 / Game 50651-50700 / Battle 50701-50750 / Global 50751-50800
//
// 注: 原 50051-50250 与 Windows 的 Docker/Hyper-V/WSL 保留端口段 (50000-50366)
// 重叠, 会导致 Login/Game 的 RPC 服务端 bind 静默失败 (50060-50159 被系统排除),
// 故整体上移到 50551+ 空闲段。
// =============================================================================
inline uint16 GetBasePortForType(EProcessType type)
{
    switch (type)
    {
    case EProcessType_PGate:
        return 50551;
    case EProcessType_PLogin:
        return 50601;
    case EProcessType_PGlobal:
        return 50751;
    case EProcessType_PGame:
        return 50651;
    case EProcessType_PBattle:
        return 50701;
    default:
        return 0; // 未知进程类型: 0 = 无效端口, 由调用方显式校验
    }
}

// =============================================================================
// 验证 Key 派生 — 由进程 ID + 固定密钥派生 (可离线校验)
//
// HMAC-SHA256(key=secret, msg=processId) → 64 字符小写 hex。密钥不可从
// authKey 反推, 比旧 FNV-1a 具备密钥隔离性。密钥来源不变: server.xml
// <auth secret="..."/> (默认 RealServerAuthSecret)。
// =============================================================================
inline std::string GenerateAuthKey(int64 processId, const std::string &secret)
{
    return xHmacSha256::HmacSha256Hex(secret, std::to_string(processId));
}

// =============================================================================
// 进程间链接规则
// =============================================================================
struct LinkRule
{
    EProcessType fromType;
    std::vector<EProcessType> toTypes; // fromType 需要链接到哪些类型
};

// =============================================================================
// 对等连接信息
// =============================================================================
struct PeerConnection
{
    ProcessInfo info;
    std::string channelName; // 连接标识 (asio-TCP 后为 "rpc:<processId>", 非 gRPC channel)
    int64 connectedAt = 0;   // 连接建立时间戳
    bool available = false;  // 是否可用于 Actor 分配
};

// =============================================================================
// RPC 调用错误 — 错误码 + 详细描述
// =============================================================================
struct RpcError
{
    int32 code = 0;     // xErrorCode 错误码
    std::string detail; // 例如: "节点 Gate:1 @ 10.0.0.5:50051 非运行状态, 当前=Paused"
    std::string targetProcess; // 目标进程标识

    bool IsOk() const
    {
        return code == 0;
    }

    std::string ToString() const
    {
        char buf[512] = {0};
        snprintf(buf, sizeof(buf), "[RpcError code=%d] %s (target=%s)", code, detail.c_str(),
                 targetProcess.c_str());
        return buf;
    }
};

inline RpcError MakeRpcError(int32 code, const std::string &detail, const std::string &target = "")
{
    RpcError e;
    e.code = code;
    e.detail = detail;
    e.targetProcess = target;
    return e;
}
