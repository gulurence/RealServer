#include "CActorLoadManager.h"

#include "CPlayerActorIndex.h"
#include "xActor/ActorMgr.h"
#include "xConfig/xServerConfig.h"
#include "xLinkManager.h"
#include "xServiceDiscovery.h"
#include "xRpc/CRpcTcpClient.h"
#include "xRpc/RpcFrame.h"
#include "rpc/actor_mgr.pb.h"
#include "xLog/xLog.h"

#include <chrono>
#include <thread>

// =============================================================================
// Init — 从启动进程类型拉取「负载类型」并注册
//
// 取代旧的 Lua 侧 hostType() 逐条注册模式。承载关系由 GetHostedActorTypes
// (xProcessDefine.h) 按进程类型静态声明, 此处直接展开注册进 CActorMgr。
// =============================================================================
void CActorLoadManager::Init(EProcessType selfType)
{
    auto hosted = CServerConfig::getMe().GetHostedActorTypes(selfType);
    XCLOG("[ActorLoad] Init: process=%s hosted_types=%zu", ProcessTypeName(selfType),
          hosted.size());

    for (EActorType type : hosted)
    {
        CActorMgr::instance()->AddHostedType(type);
        XCLOG("[ActorLoad] Registered load type=%d (process can host it)", (int32)type);
    }

    // 初始化 player:{id} → actorId 路由索引 (复用服务发现 Redis 池)
    std::string poolTitle = CServerConfig::getMe().GetConfig().discoveryRedis.poolTitle;
    CPlayerActorIndex::instance()->Init(poolTitle);
}

bool CActorLoadManager::CanHostType(EActorType t) const
{
    return CActorMgr::instance()->CanHostType(t);
}

// =============================================================================
// PickProcessForActor — 负载均衡挑承载进程
//
// Actor 类型 → 承载它的进程类型 → CLinkManager::GetBestPeer (最低 loadScore 且 Running)。
// =============================================================================
ProcessInfo CActorLoadManager::PickProcessForActor(EActorType t) const
{
    EProcessType procType = ActorTypeToProcessType(t);
    if (procType == EProcessType_None)
    {
        XWRN("[ActorLoad] PickProcess: no process type hosts actor type=%d", (int32)t);
        return ProcessInfo{};
    }

    ProcessInfo peer = CLinkManager::instance()->GetBestPeer(procType);
    if (peer.processId == 0)
    {
        XWRN("[ActorLoad] PickProcess: no available %s process for actor type=%d",
             ProcessTypeName(procType), (int32)t);
    }
    return peer;
}

// =============================================================================
// ResolveActorProcess — 按身份定位 Actor 所在进程 (灰度迁移后消息须追到新进程)
//
// 多实例 Actor (Player/Battle/Spectator, id!=0) 迁移后, 其 cluster:actor:{type}:{id}
// 的 pid 字段已由新进程 RegisterActor 更新; 按身份定位才能把消息切到新进程执行,
// 而不是负载均衡地打到「负载最低」的进程 (可能不承载该 Actor, 接收端 Find 落空)。
// 单例 (id==0) 与未注册/已驱逐(迁移中)者回落负载均衡。
// =============================================================================
ProcessInfo CActorLoadManager::ResolveActorProcess(EActorType t, ActorID id) const
{
    if (id != 0)
    {
        int64 pid = CServiceDiscovery::getMe().GetActorProcessId(t, id);
        // IsPeerAvailable: 跳过已死/心跳超时的对端 (否则会路由到僵尸进程再等 RPC 失败);
        // 排空中的进程仍 available=true, 故此处不会误排 (既有战斗消息仍要路由到它)。
        if (pid != 0 && CLinkManager::instance()->IsPeerAvailable(pid))
        {
            ProcessInfo peer = CLinkManager::instance()->GetPeerInfo(pid);
            if (peer.processId != 0)
            {
                return peer;
            }
        }
    }
    return PickProcessForActor(t);
}

// =============================================================================
// CreateActorRemote — 挑进程 + 发 RPC 创建
// =============================================================================
bool CActorLoadManager::CreateActorRemote(EActorType t, ActorID id, std::string *outIp,
                                          uint16 *outPort)
{
    ProcessInfo peer = PickProcessForActor(t);
    if (peer.processId == 0)
    {
        XERR("[ActorLoad] CreateRemote: no available process for actor type=%d", (int32)t);
        return false;
    }

    actormgr::PBRPCCreateActorRequest req;
    actormgr::PBRPCCreateActorReply reply;
    req.set_actor_type((int32)t);
    req.set_actor_id(id);

    auto result
        = CRpcTcpClient::instance()
              ->CallSync<actormgr::PBRPCCreateActorRequest, actormgr::PBRPCCreateActorReply>(
                  peer.processId, RPC_METHOD_CREATE_ACTOR, req);
    if (!result.ok || !result.reply.success())
    {
        XERR("[ActorLoad] CreateRemote failed: transport_ok=%d code=%d", (int32)result.ok,
             result.code);
        return false;
    }
    if (outIp)
    {
        *outIp = peer.ip;
    }
    if (outPort)
    {
        *outPort = peer.listenPort;
    }
    XLOG("[ActorLoad] CreateRemote ok: type=%d id=%lld on process=%lld ip=%s listen=%u",
         (int32)t, (int64)id, (int64)peer.processId, peer.ip.c_str(), peer.listenPort);
    return true;
}

// =============================================================================
// FindActorRemote — 挑承载进程 + 发 RPC 查询
// outFound / outRemoteType 始终被赋值
// =============================================================================
bool CActorLoadManager::FindActorRemote(EActorType t, ActorID id, bool *outFound,
                                        int32 *outRemoteType)
{
    if (outFound)
    {
        *outFound = false;
    }
    if (outRemoteType)
    {
        *outRemoteType = 0;
    }

    ProcessInfo peer = ResolveActorProcess(t, id);
    if (peer.processId == 0)
    {
        return false;
    }

    actormgr::PBRPCFindActorRequest req;
    actormgr::PBRPCFindActorReply reply;
    req.set_actor_type((int32)t);
    req.set_actor_id(id);

    auto result = CRpcTcpClient::instance()
                      ->CallSync<actormgr::PBRPCFindActorRequest, actormgr::PBRPCFindActorReply>(
                          peer.processId, RPC_METHOD_FIND_ACTOR, req);
    if (!result.ok)
    {
        return false;
    }
    if (outFound)
    {
        *outFound = result.reply.found();
    }
    if (outRemoteType)
    {
        *outRemoteType = result.reply.remote_type();
    }
    return result.reply.found();
}

// =============================================================================
// UninstallActorRemote — 挑承载进程 + 发 RPC 卸载
// =============================================================================
bool CActorLoadManager::UninstallActorRemote(EActorType t, ActorID id)
{
    ProcessInfo peer = ResolveActorProcess(t, id);
    if (peer.processId == 0)
    {
        return false;
    }

    actormgr::PBRPCUninstallActorRequest req;
    actormgr::PBRPCUninstallActorReply reply;
    req.set_actor_type((int32)t);
    req.set_actor_id(id);

    auto result
        = CRpcTcpClient::instance()
              ->CallSync<actormgr::PBRPCUninstallActorRequest, actormgr::PBRPCUninstallActorReply>(
                  peer.processId, RPC_METHOD_UNINSTALL_ACTOR, req);
    if (!result.ok || !result.reply.success())
    {
        XERR("[ActorLoad] UninstallRemote failed: transport_ok=%d code=%d", (int32)result.ok,
             result.code);
        if (!result.ok)
        {
            // 传输失败: 立即摘除疑似已死的对端 (灰度强杀后的即时自愈)
            CLinkManager::instance()->DisconnectFromPeer(peer.processId);
        }
        return false;
    }
    return true;
}

// =============================================================================
// LoginRemote — 挑 Login 进程 + 发 RPC_METHOD_LOGIN 转发登录链路
// =============================================================================
std::string CActorLoadManager::LoginRemote(int32 op, const std::string &payload)
{
    ProcessInfo peer = CLinkManager::instance()->GetBestPeer(EProcessType_PLogin);
    if (peer.processId == 0)
    {
        XERR("[LoginRpc] no available Login process");
        return "";
    }

    std::string reqBody;
    reqBody.reserve(1 + payload.size());
    reqBody.push_back((char)(op & 0xFF));
    reqBody.append(payload);

    auto result = CRpcTcpClient::instance()->CallSyncRaw(peer.processId, RPC_METHOD_LOGIN, reqBody);
    if (!result.ok)
    {
        XERR("[LoginRpc] failed: transport_ok=%d code=%d", (int32)result.ok, result.code);
        // 传输失败: 立即摘除疑似已死的对端 (灰度强杀旧 Login 后避免一直选中死连接, 下次改选新进程)
        CLinkManager::instance()->DisconnectFromPeer(peer.processId);
        return "";
    }
    return result.rspBody;
}

// =============================================================================
// ForwardToActor — 挑承载进程 + 发 RPC_METHOD_FORWARD_ACTOR 通用转发
//
// body = [int32 actor_type][int64 actor_id][客户端 protobuf 负载]
// 目标进程按 (type, id) 定位 Actor, 派发其 Lua on_rpc_forward, 返回客户端回包。
// =============================================================================
std::string CActorLoadManager::ForwardToActor(EActorType t, ActorID id, const std::string &payload)
{
    // 迁移窗口内有界重试: 灰度「先迁后杀」在 saveAll 驱逐 cluster:actor:{type}:{id} 后、
    // 新进程 RegisterActor 之前, pid 短暂为空。此处在途消息按身份重查并短暂等待 Actor
    // 重现, 而非立即回落负载均衡 (可能打到不承载该 Actor 的进程而丢包) —— 客户端无感切换。
    // 正常路径一次 Redis HGET 即命中, 无额外延迟; 重试仅在 id!=0 且身份未命中时触发。
    constexpr int32 kMigrateResolveRetry = 5;
    constexpr int32 kMigrateResolveRetryMs = 200;

    ProcessInfo peer;
    if (id != 0)
    {
        for (int32 attempt = 0; attempt < kMigrateResolveRetry; ++attempt)
        {
            int64 pid = CServiceDiscovery::getMe().GetActorProcessId(t, id);
            if (pid != 0 && CLinkManager::instance()->IsPeerAvailable(pid))
            {
                peer = CLinkManager::instance()->GetPeerInfo(pid);
                if (peer.processId != 0)
                {
                    break;
                }
            }
            peer = ProcessInfo{};
            if (attempt + 1 < kMigrateResolveRetry)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(kMigrateResolveRetryMs));
            }
        }
    }
    if (peer.processId == 0)
    {
        peer = PickProcessForActor(t);
    }
    if (peer.processId == 0)
    {
        XERR("[ForwardRpc] no available process for actor type=%d", (int32)t);
        return "";
    }

    // 12 字节头: [int32 type][int64 id] (小端, 与 si_net_body_cid 约定一致)
    std::string reqBody;
    reqBody.reserve(12 + payload.size());
    for (int32 i = 0; i < 4; ++i)
    {
        reqBody.push_back((char)(((int32)t >> (8 * i)) & 0xFF));
    }
    for (int32 i = 0; i < 8; ++i)
    {
        reqBody.push_back((char)(((int64)id >> (8 * i)) & 0xFF));
    }
    reqBody.append(payload);

    auto result
        = CRpcTcpClient::instance()->CallSyncRaw(peer.processId, RPC_METHOD_FORWARD_ACTOR, reqBody);
    if (!result.ok)
    {
        // code=1 为目标 Actor 已被卸载 (Battle/Spectator 结算清理窗口内的在途
        // INPUT_REQ), 属正常落空, 降级警告; code=2 为超时, 保持错误级别。
        if (result.code == 1)
        {
            XWRN("[ForwardRpc] target actor gone: type=%d id=%lld", (int32)t, (int64)id);
        }
        else
        {
            XERR("[ForwardRpc] failed: transport_ok=%d code=%d type=%d", (int32)result.ok,
                 result.code, (int32)t);
            // 传输失败: 立即摘除疑似已死的对端 (灰度强杀后的即时自愈, 下次改选其它进程)
            CLinkManager::instance()->DisconnectFromPeer(peer.processId);
        }
        return "";
    }
    return result.rspBody;
}

// =============================================================================
// PushToGate — 挑 Gate 进程 + 发 RPC_METHOD_PUSH 主动推送 (异步, 无回包)
//
// body = [int64 playerId][客户端 protobuf 负载]
// Gate 侧 RPC_METHOD_PUSH handler 派发给其 Lua on_rpc_push, 按 playerId 反查
// 连接 CID 后 si_net_send 下发。用 SubmitRaw 异步发送, 避免在 Actor/调度线程上
// 阻塞等待 (推送无业务回包, 服务器仍会回传输层响应, 客户端侧由无操作回调清理)。
// =============================================================================
void CActorLoadManager::PushToGate(int64 playerId, const std::string &payload)
{
    ProcessInfo peer = CLinkManager::instance()->GetBestPeer(EProcessType_PGate);
    if (peer.processId == 0)
    {
        XWRN("[PushRpc] no available Gate process, drop push player=%lld", (int64)playerId);
        return;
    }

    std::string reqBody;
    reqBody.reserve(8 + payload.size());
    for (int32 i = 0; i < 8; ++i)
    {
        reqBody.push_back((char)((playerId >> (8 * i)) & 0xFF));
    }
    reqBody.append(payload);

    CRpcTcpClient::instance()->SubmitRaw(peer.processId, RPC_METHOD_PUSH, reqBody);
}

// =============================================================================
// KickGateClient — 定向踢下线 (同账号并发登录「踢旧允许新」)
//
// body = [int64 cid] (小端)。发到指定 processId 的 Gate 进程, 其 RPC_METHOD_KICK
// handler 直接断开该连接。异步无回包 (踢下线无业务响应)。
// =============================================================================
void CActorLoadManager::KickGateClient(int64 processId, int64 cid)
{
    if (processId <= 0 || cid <= 0)
    {
        return;
    }

    std::string reqBody;
    reqBody.reserve(8);
    for (int32 i = 0; i < 8; ++i)
    {
        reqBody.push_back((char)((cid >> (8 * i)) & 0xFF));
    }

    CRpcTcpClient::instance()->SubmitRaw(processId, RPC_METHOD_KICK, reqBody);
}

// =============================================================================
// GetLocalLoad — 本进程负载 (actor 实例总数)
// =============================================================================
int32 CActorLoadManager::GetLocalLoad() const
{
    return (int32)CActorMgr::instance()->GetInstanceCount();
}
