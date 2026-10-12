#pragma once

#include "xBase/xDefine.h"

#include <functional>
#include <string>

// =============================================================================
// RpcFrame.h — 自建 asio-TCP RPC 帧定义 (gRPC 退役后使用)
//
// 帧格式 (主机字节序, 与 xNetAgent 的 NetPackageHeadST 一致, 同机集群场景):
//   请求:   [uint32 bodyLen][uint32 requestId][uint32 methodId][uint32 fromProcessId][protobuf
//   body] 响应:   [uint32 bodyLen][uint32 requestId][uint32 errCode  ][uint32
//   fromProcessId][protobuf body]
//
// 第三个 uint32 字段: 请求侧是 methodId, 响应侧是 errCode(0=成功)。
// 第四个 uint32 字段 fromProcessId: 发送方进程 ID, 供服务端识别对端 (链路追踪 Recv 方向)。
// =============================================================================

// RPC 方法 ID — 跨进程 Actor 管理 (迁移自 actor_mgr.proto 的 gRPC service)
enum ERpcMethodId : uint32
{
    RPC_METHOD_NONE = 0,
    RPC_METHOD_CREATE_ACTOR = 1,    // 远程创建 Actor
    RPC_METHOD_FIND_ACTOR = 2,      // 远程查询 Actor
    RPC_METHOD_UNINSTALL_ACTOR = 3, // 远程卸载 Actor
    RPC_METHOD_LOGIN = 4,           // 登录链路转发 (Gate→Login, body=[op][payload])
    RPC_METHOD_FORWARD_ACTOR = 5, // 通用业务转发 (Gate→任意 Actor, body=[type][id][payload])
    RPC_METHOD_PUSH = 6, // 服务器主动推送 (业务Actor→Gate, body=[int64 playerId][payload])
};

// 单帧最大 body 长度 (安全上限)
#define MAX_RPC_BODY_SIZE (1 << 22) // 4MB

#pragma pack(1)
struct RpcFrameHeadST
{
    uint32 u32Len;           // body 长度 (不含本头部 16 字节)
    uint32 u32RequestId;     // 请求关联 ID (响应时原样回传)
    uint32 u32MethodId;      // 请求: method id; 响应: 0=ok, 否则 errCode
    uint32 u32FromProcessId; // 发送方进程 ID (服务端据此识别对端, 供链路追踪)
};
#pragma pack()

#define RpcFrameHeadSize sizeof(RpcFrameHeadST)

// 服务端 handler 结果: 传输错误码(0=ok) + 序列化后的响应 body
struct RpcHandlerResult
{
    int32 errCode = 0;
    std::string rspBody;
};

// 服务端 handler: (请求 body 字节) -> (传输错误码, 响应 body 字节)
using RpcHandler = std::function<RpcHandlerResult(const std::string &reqBody)>;

// 客户端一次 RPC 调用的结果 (lambda 回调参数)
template <typename TReply> struct RpcCallResult
{
    bool ok = false; // 传输成功 (code == 0)
    int32 code = 0;  // 响应头里的 errCode (0 = 传输成功; 业务错误在 reply 里)
    TReply reply;    // 反序列化后的响应 (code==0 时有效)
};

// 原始字节 RPC 调用结果 (不反序列化 protobuf, rspBody 为原始响应字节)
struct RawRpcResult
{
    bool ok = false;     // 传输成功 (code == 0)
    int32 code = 0;      // 响应头里的 errCode (0 = 传输成功)
    std::string rspBody; // 原始响应字节 (code==0 时有效)
};
