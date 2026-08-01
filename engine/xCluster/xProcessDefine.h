#pragma once

#include "xBase/xDefine.h"
#include "xLog/xLog.h"
#include <string>
#include <map>

// =============================================================================
// 集群链接状态变化日志 — 绿色显示
// =============================================================================
#define CLUSTER_GREEN  "\033[32m"
#define CLUSTER_RESET  "\033[0m"

// 绿色日志宏 — 用于进程链接/发现/状态变化
#define XCLOG(fmt, ...) \
    XLOG(CLUSTER_GREEN "[CLUSTER] " fmt CLUSTER_RESET, ##__VA_ARGS__)
#define XCWRN(fmt, ...) \
    XWRN(CLUSTER_GREEN "[CLUSTER] " fmt CLUSTER_RESET, ##__VA_ARGS__)
#define XCERR(fmt, ...) \
    XERR(CLUSTER_GREEN "[CLUSTER] " fmt CLUSTER_RESET, ##__VA_ARGS__)
#include <vector>

// =============================================================================
// 进程类型 — 与 ActorType 共用值域，便于 Actor 路由
// =============================================================================
enum EProcessType : int32 {
    EProcessType_None   = 0,
    EProcessType_Gate   = 100,    // 网关进程
    EProcessType_Login  = 101,    // 登录进程
    EProcessType_Player = 201,    // 玩家进程
    EProcessType_World  = 202,    // 世界进程
    EProcessType_Max
};

// 从 ActorType 转换
inline EProcessType ActorTypeToProcessType(int32 actorType) {
    switch (actorType) {
        case 100: return EProcessType_Gate;
        case 101: return EProcessType_Login;
        case 201: return EProcessType_Player;
        case 202: return EProcessType_World;
        default:  return EProcessType_None;
    }
}

inline const char* ProcessTypeName(EProcessType t) {
    switch (t) {
        case EProcessType_Gate:   return "Gate";
        case EProcessType_Login:  return "Login";
        case EProcessType_Player: return "Player";
        case EProcessType_World:  return "World";
        default:                  return "Unknown";
    }
}

// =============================================================================
// 进程运行状态
// =============================================================================
enum EProcessState : int32 {
    EProcessState_Starting    = 0,    // 启动中
    EProcessState_Init        = 1,    // 初始化完成
    EProcessState_Running     = 2,    // 运行中
    EProcessState_Paused      = 3,    // 暂停
    EProcessState_Stopping    = 4,    // 停用中
    EProcessState_Shutdown    = 5,    // 已关闭
};

inline const char* ProcessStateName(EProcessState s) {
    switch (s) {
        case EProcessState_Starting: return "Starting";
        case EProcessState_Init:     return "Init";
        case EProcessState_Running:  return "Running";
        case EProcessState_Paused:   return "Paused";
        case EProcessState_Stopping: return "Stopping";
        case EProcessState_Shutdown: return "Shutdown";
        default:                     return "Unknown";
    }
}

inline bool IsProcessRunning(EProcessState s) {
    return s == EProcessState_Running;
}

// =============================================================================
// 进程身份信息
// =============================================================================
struct ProcessInfo {
    EProcessType type          = EProcessType_None;
    int64        processId     = 0;         // Redis 自增生成的唯一进程 ID
    std::string  processName;               // 进程名称 (e.g. "gate-01")
    std::string  host          = "127.0.0.1";
    uint16       grpcPort      = 50051;
    int32        loadScore     = 0;         // 负载分数 (actor 实例数)
    EProcessState state        = EProcessState_Starting;
    int64        lastHeartbeat = 0;         // Unix 秒时间戳

    // Redis Hash 序列化 key
    std::string RedisKey() const {
        char buf[128];
        snprintf(buf, sizeof(buf), "cluster:process:%d:%lld",
                 (int32)type, (int64)processId);
        return buf;
    }
};

// =============================================================================
// 进程间链接规则
// =============================================================================
struct LinkRule {
    EProcessType fromType;
    std::vector<EProcessType> toTypes;   // fromType 需要链接到哪些类型
};

// =============================================================================
// 对等连接信息
// =============================================================================
struct PeerConnection {
    ProcessInfo  info;
    std::string  channelName;              // gRPC channel 名称 (CRpcService key)
    int64        connectedAt = 0;          // 连接建立时间戳
    bool         available   = false;      // 是否可用于 Actor 分配
};

// =============================================================================
// RPC 调用错误 — 错误码 + 详细描述
// =============================================================================
struct RpcError {
    int32       code   = 0;               // xErrorCode 错误码
    std::string detail;                    // 例如: "节点 Gate:1 @ 10.0.0.5:50051 非运行状态, 当前=Paused"
    std::string targetProcess;             // 目标进程标识

    bool IsOk() const { return code == 0; }

    std::string ToString() const {
        char buf[512];
        snprintf(buf, sizeof(buf), "[RpcError code=%d] %s (target=%s)",
                 code, detail.c_str(), targetProcess.c_str());
        return buf;
    }
};

inline RpcError MakeRpcError(int32 code, const std::string& detail,
                              const std::string& target = "") {
    RpcError e;
    e.code   = code;
    e.detail = detail;
    e.targetProcess = target;
    return e;
}
