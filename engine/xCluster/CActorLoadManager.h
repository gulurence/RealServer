/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: CActorLoadManager.h
      Purpose:   Actor 负载管理器接口声明：Actor 类型到承载进程的定位与跨进程创建/转发入口。

      声明 CActorLoadManager 单例（继承 xSingleton），负责启动注册可承载类型、按负载挑进程、远程创建/查询/卸载 Actor
      含按身份定位 ResolveActorProcess 与灰度迁移窗口有界重试的 ResolveActorProcessForForward
      转发/推送/踢下线均为向目标进程发 asio-TCP RPC，单例状态委托 CActorMgr，不另造真值源
  *****************************************************************/

#pragma once

#include "xProcessDefine.h"
#include "xActor/ActorDefine.h"
#include "xBase/xSingleton.h"

// =============================================================================
// CActorLoadManager — 统一的 Actor 负载管理器
//
// 职责 (Actor 负载的唯一入口):
//   1. 启动时从进程类型拉取可承载的 Actor 类型 (负载类型) 并注册到 CActorMgr
//   2. 给定 Actor 类型 → 均衡负载挑一个可用承载进程 (PickProcessForActor)
//   3. 跨进程 Actor 创建/查询/卸载 (向选定进程发 RPC)
//   4. 上报本进程负载 (供心跳)
//
// 与 CLinkManager 的分工:
//   - CLinkManager 负责「链接/发现」: 维护 m_mapPeers、GetBestPeer/GetAvailablePeers
//   - 本管理器负责「负载策略 + Actor 创建」: 把 Actor 类型映射到承载进程并完成创建
//
// 单例状态全部委托给 CActorMgr (hosted types / instance count), 不另造真值源。
// =============================================================================
class CActorLoadManager : public xSingleton<CActorLoadManager>
{
public:
    CActorLoadManager()
    {
    }
    ~CActorLoadManager()
    {
    }

    // 启动时调用: 从启动进程类型拉取可承载的 Actor 类型, 注册到 CActorMgr
    void Init(EProcessType selfType);

    // 本进程是否承载该 Actor 类型 (委托 CActorMgr::CanHostType)
    bool CanHostType(EActorType t) const;

    // 负载均衡: 找承载该类型的最低负载可用进程
    // 内部: ActorTypeToProcessType → CLinkManager::GetBestPeer
    ProcessInfo PickProcessForActor(EActorType t) const;

    // 远程创建: 挑进程 + 发 RPC_METHOD_CREATE_ACTOR
    // 成功时输出承载进程的 ip / listenPort (供客户端直连 Battle 等场景)
    bool CreateActorRemote(EActorType t, ActorID id, std::string *outIp = nullptr,
                           uint16 *outPort = nullptr);

    // 远程查询: outFound/outRemoteType 始终被赋值
    bool FindActorRemote(EActorType t, ActorID id, bool *outFound, int32 *outRemoteType);

    // 远程卸载: 挑承载进程 + 发 RPC_METHOD_UNINSTALL_ACTOR
    bool UninstallActorRemote(EActorType t, ActorID id);

    // 登录链路转发: 挑 Login 进程 + 发 RPC_METHOD_LOGIN
    // op: 1=login, 2=select_role; payload: 客户端 protobuf 负载
    // @return Login 进程返回的响应 protobuf (失败/超时返回空串)
    std::string LoginRemote(int32 op, const std::string &payload);

    // 通用业务转发: 挑承载该 Actor 类型的进程 + 发 RPC_METHOD_FORWARD_ACTOR
    // 目标进程按 (type, id) 定位 Actor, 把 payload 派发给其 Lua on_rpc_forward。
    // @return 目标 Actor 返回的客户端回包 protobuf (失败/超时返回空串)
    std::string ForwardToActor(EActorType t, ActorID id, const std::string &payload);

    // 通用业务转发的异步版 (fire-and-forget, 无回包): 与 ForwardToActor 相同的进程定位,
    // 但用 SubmitRaw 异步发送, 不阻塞调用线程。供「通知类」转发 (如选角后置 Player online=1)
    // 使用 —— 其回包为空串且调用方不消费, 阻塞等待是无谓的登录热路径延迟。
    void ForwardToActorAsync(EActorType t, ActorID id, const std::string &payload);

    // 服务器主动推送: 向持有该玩家连接的 Gate 进程发 RPC_METHOD_PUSH (异步, 无回包)。
    // payload 为客户端可直接接收的业务 protobuf (Gate 侧按 playerId → cid 转发)。
    void PushToGate(int64 playerId, const std::string &payload);

    // 定向踢下线: 向指定 processId 的 Gate 进程发 RPC_METHOD_KICK, 断开连接 cid
    // (同账号并发登录「踢旧允许新」)。异步无回包。
    void KickGateClient(int64 processId, int64 cid);

    // 本进程负载 (供心跳上报)
    int32 GetLocalLoad() const;

private:
    // 按 (type, id) 定位 Actor 所在进程:
    //   id!=0 → 读 cluster:actor:{type}:{id} 的 pid 字段 (灰度迁移后 Actor 落到新进程),
    //           命中且对端在线则返回该进程; 否则回落 PickProcessForActor (负载均衡)。
    //   id==0 → 单例, 直接负载均衡 (每个承载进程都持有单例)。
    ProcessInfo ResolveActorProcess(EActorType t, ActorID id) const;

    // 转发定位 (同步/异步转发共用): 在 ResolveActorProcess 基础上叠加灰度迁移窗口内的
    // 有界重试 —— 迁移「先迁后杀」saveAll 驱逐 actor 索引后、新进程 RegisterActor 之前 pid
    // 短暂为空, 此处短暂等待 Actor 重现而非立即回落负载均衡。
    ProcessInfo ResolveActorProcessForForward(EActorType t, ActorID id) const;
};
