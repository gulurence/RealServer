#pragma once

#include <string>

// =============================================================================
// 框架统一错误码
// =============================================================================
enum xErrorCode : int32
{
    // ---- 成功 ----
    xErrorCode_Success = 0,

    // =======================================================================
    // Actor (1 ~ 19)
    // =======================================================================
    xErrorCode_DatabaseNotFindActor = 1,     // Database 中未找到 Actor 数据
    xErrorCode_RedisNotFindActor    = 2,     // Redis 中未找到 Actor 数据
    xErrorCode_ActorCreateFailed    = 3,     // Actor 创建失败
    xErrorCode_ActorInstallFailed   = 4,     // Actor 安装失败
    xErrorCode_ActorNotHosted       = 5,     // 本进程不承载该 Actor 类型

    // =======================================================================
    // Cluster / Service Discovery (100 ~ 119)
    // =======================================================================
    xErrorCode_NodeNotFound          = 100,  // 节点未找到
    xErrorCode_NodeNotRunning        = 101,  // 节点非运行状态
    xErrorCode_NodeConnectionFailed  = 102,  // 节点连接失败
    xErrorCode_NodeTimeout           = 103,  // 节点超时
    xErrorCode_ServiceRegisterFailed = 104,  // 服务注册失败
    xErrorCode_RedisUnavailable      = 105,  // Redis 不可用
    xErrorCode_ProcessCreateFailed   = 106,  // 进程创建失败
    xErrorCode_HeartbeatFailed       = 107,  // 心跳更新失败

    // =======================================================================
    // RPC (200 ~ 219)
    // =======================================================================
    xErrorCode_RpcCallFailed        = 200,   // RPC 调用失败
    xErrorCode_RpcChannelBroken     = 201,   // RPC 通道断开
    xErrorCode_RpcTimeout           = 202,   // RPC 超时
    xErrorCode_RpcSerializeFailed   = 203,   // 序列化/反序列化失败
    xErrorCode_RpcNoAvailablePeer   = 204,   // 无可用的对等节点

    // =======================================================================
    // Redis (300 ~ 319)
    // =======================================================================
    xErrorCode_RedisConnectFailed   = 300,   // Redis 连接失败
    xErrorCode_RedisCommandFailed   = 301,   // Redis 命令执行失败

    // =======================================================================
    // Database (400 ~ 419)
    // =======================================================================
    xErrorCode_DBConnectFailed      = 400,   // 数据库连接失败
    xErrorCode_DBQueryFailed        = 401,   // 数据库查询失败
    xErrorCode_DBDataTooLarge       = 402,   // 数据超过大小限制

    // =======================================================================
    // Config (500 ~ 519)
    // =======================================================================
    xErrorCode_ConfigParseFailed    = 500,   // 配置文件解析失败
    xErrorCode_ConfigNotFound       = 501,   // 配置文件未找到
};

// =============================================================================
// 获取错误码描述字符串 (中文)
// =============================================================================
inline const char* GetErrorString(xErrorCode code) {
    switch (code) {
        case xErrorCode_Success:                return "成功";

        case xErrorCode_DatabaseNotFindActor:   return "Database 未找到 Actor";
        case xErrorCode_RedisNotFindActor:      return "Redis 未找到 Actor";
        case xErrorCode_ActorCreateFailed:      return "Actor 创建失败";
        case xErrorCode_ActorInstallFailed:     return "Actor 安装失败";
        case xErrorCode_ActorNotHosted:         return "进程不承载该 Actor 类型";

        case xErrorCode_NodeNotFound:           return "集群节点未找到";
        case xErrorCode_NodeNotRunning:         return "集群节点非运行状态";
        case xErrorCode_NodeConnectionFailed:   return "集群节点连接失败";
        case xErrorCode_NodeTimeout:            return "集群节点超时";
        case xErrorCode_ServiceRegisterFailed:  return "服务注册失败";
        case xErrorCode_RedisUnavailable:       return "Redis 不可用";
        case xErrorCode_ProcessCreateFailed:    return "进程创建失败";
        case xErrorCode_HeartbeatFailed:        return "心跳更新失败";

        case xErrorCode_RpcCallFailed:          return "RPC 调用失败";
        case xErrorCode_RpcChannelBroken:       return "RPC 通道断开";
        case xErrorCode_RpcTimeout:             return "RPC 超时";
        case xErrorCode_RpcSerializeFailed:     return "RPC 序列化失败";
        case xErrorCode_RpcNoAvailablePeer:     return "无可用对等节点";

        case xErrorCode_RedisConnectFailed:     return "Redis 连接失败";
        case xErrorCode_RedisCommandFailed:     return "Redis 命令执行失败";

        case xErrorCode_DBConnectFailed:        return "数据库连接失败";
        case xErrorCode_DBQueryFailed:          return "数据库查询失败";
        case xErrorCode_DBDataTooLarge:         return "数据超过大小限制";

        case xErrorCode_ConfigParseFailed:      return "配置文件解析失败";
        case xErrorCode_ConfigNotFound:         return "配置文件未找到";

        default:                                return "未知错误";
    }
}
