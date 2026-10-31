/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: ActorRpcHandlers.cpp
      Purpose:   Actor 管理 RPC 方法注册

      RegisterActorRpcHandlers 注册 CreateActor/FindActor/UninstallActor/Login/ForwardActor/Push/Kick 七个方法
      创建/卸载/转发会触碰 lua_State, 经 CActorScheduler::DispatchSync 或 SYS_RPC_PUSH 投递调度线程串行执行
      复用 actor_mgr.proto 的 actormgr::* 消息结构, 不再走 gRPC stub
  *****************************************************************/

#include "ActorRpcHandlers.h"
#include "RpcFrame.h"
#include "CRpcTcpServer.h"
#include "rpc/actor_mgr.pb.h"
#include "xActor/ActorCreator.h"
#include "xActor/ActorMgr.h"
#include "xActor/ActorMsgId.h"
#include "xActor/ActorScheduler.h"
#include "xCluster/xProcessDefine.h"
#include "luacore/CLuaActor.h"
#include "xNet/CNetGateway.h"
#include "xLog/xLog.h"

void RegisterActorRpcHandlers()
{
    CRpcTcpServer *server = CRpcTcpServer::instance();

    // --- CreateActor ---
    server->Register(RPC_METHOD_CREATE_ACTOR,
                     [](const std::string &reqBody) -> RpcHandlerResult
                     {
                         RpcHandlerResult result;
                         actormgr::PBRPCCreateActorRequest req;
                         actormgr::PBRPCCreateActorReply reply;
                         if (!req.ParseFromString(reqBody))
                         {
                             result.errCode = 1;
                             return result;
                         }

                         EActorType type = (EActorType)req.actor_type();
                         int64 id = req.actor_id();
                         XLOG("[ActorRpc] CreateActor: type=%d id=%lld", type, (int64)id);

                         if (!CActorMgr::instance()->CanHostType(type))
                         {
                             XWRN("[ActorRpc] CreateActor: type=%d not hosted here", type);
                             reply.set_success(false);
                             reply.set_error_code(1); // not hosted
                         }
                         else if (CActorMgr::instance()->Find(type, id))
                         {
                             // 幂等: 该 (type,id) Actor 已存在 (断线重连/重复选角),
                             // 直接复用而非重复创建。
                             XLOG("[ActorRpc] CreateActor: type=%d id=%lld already exists, reuse",
                                  type, (int64)id);
                             reply.set_success(true);
                             reply.set_error_code(0);
                         }
                         else
                         {
                             // 创建/安装会触碰 lua_State (CLuaActor::Create 的 class:new、
                             // Install 的 onInstall), 必须投递调度线程串行执行, 避免与消息
                             // 循环并发。返回错误码: 0=成功, 2/3/4 同原语义。
                             int32 ec = CActorScheduler::instance()->DispatchSync(
                                 [type, id]() -> int32
                                 {
                                     CActor *pActor = ActorCreator::Create(type, id);
                                     if (!pActor)
                                     {
                                         return 2; // create failed
                                     }
                                     if (!CActorMgr::instance()->AddInstance(pActor))
                                     {
                                         return 3; // add failed
                                     }
                                     if (!CActorMgr::instance()->Install(type, id))
                                     {
                                         CActorMgr::instance()->RemoveInstance(type, id);
                                         return 4; // install failed
                                     }
                                     return 0;
                                 });

                             reply.set_success(ec == 0);
                             reply.set_error_code(ec);
                             if (ec == 0)
                             {
                                 XLOG("[ActorRpc] CreateActor: success type=%d id=%lld", type,
                                      (int64)id);
                             }
                         }

                         reply.SerializeToString(&result.rspBody);
                         result.errCode = 0;
                         return result;
                     });

    // --- FindActor ---
    server->Register(RPC_METHOD_FIND_ACTOR,
                     [](const std::string &reqBody) -> RpcHandlerResult
                     {
                         RpcHandlerResult result;
                         actormgr::PBRPCFindActorRequest req;
                         actormgr::PBRPCFindActorReply reply;
                         if (!req.ParseFromString(reqBody))
                         {
                             result.errCode = 1;
                             return result;
                         }

                         EActorType type = (EActorType)req.actor_type();
                         int64 id = req.actor_id();
                         CActor *pActor = CActorMgr::instance()->Find(type, id);
                         if (pActor)
                         {
                             reply.set_found(true);
                             reply.set_remote_type(static_cast<int32>(pActor->GetActorType()));
                         }
                         else
                         {
                             reply.set_found(false);
                             reply.set_remote_type(0);
                         }

                         reply.SerializeToString(&result.rspBody);
                         result.errCode = 0;
                         return result;
                     });

    // --- UninstallActor ---
    server->Register(RPC_METHOD_UNINSTALL_ACTOR,
                     [](const std::string &reqBody) -> RpcHandlerResult
                     {
                         RpcHandlerResult result;
                         actormgr::PBRPCUninstallActorRequest req;
                         actormgr::PBRPCUninstallActorReply reply;
                         if (!req.ParseFromString(reqBody))
                         {
                             result.errCode = 1;
                             return result;
                         }

                         EActorType type = (EActorType)req.actor_type();
                         int64 id = req.actor_id();
                         XLOG("[ActorRpc] UninstallActor: type=%d id=%lld", type, (int64)id);

                         // Find → UnInstall 一次完成 (移除注册表/Redis/恢复快照), 不再重复调用。
                         // UnInstall 会触碰 lua_State (onUnInstall), 必须投递调度线程执行。
                         // C++ 内存释放同样投递调度线程: ~CLuaActor 的 luaL_unref 需与 Lua 业务
                         // 同线程, 不在 RPC 服务线程直接 delete, 避免并发触碰 lua_State (同 PUSH)。
                         CActor *pActor = CActorMgr::instance()->Find(type, id);
                         if (!pActor)
                         {
                             reply.set_success(false);
                             reply.set_error_code(1);
                         }
                         else
                         {
                             bool ok = CActorScheduler::instance()->DispatchSync(
                                 [type, id]() -> bool
                                 { return CActorMgr::instance()->UnInstall(type, id); });
                             if (!ok)
                             {
                                 reply.set_success(false);
                                 reply.set_error_code(1);
                             }
                             else
                             {
                                 if (id != 0)
                                 {
                                     pActor->RequestStop();
                                     asio::post(CActorScheduler::instance()->GetContext(),
                                                [pActor] { delete pActor; });
                                 }
                                 reply.set_success(true);
                                 reply.set_error_code(0);
                             }
                         }

                         reply.SerializeToString(&result.rspBody);
                         result.errCode = 0;
                         return result;
                     });

    // --- Login 链路转发 (Gate→Login) ---
    // body = [1 字节 op][客户端 protobuf 负载], 响应 = 客户端回包 protobuf
    server->Register(
        RPC_METHOD_LOGIN,
        [](const std::string &reqBody) -> RpcHandlerResult
        {
            // 登录链路转发触碰 lua_State (on_rpc_login), 与 FORWARD 同理必须投递调度线程。
            return CActorScheduler::instance()->DispatchSync(
                [reqBody]() -> RpcHandlerResult
                {
                    RpcHandlerResult result;

                    CActor *pActor = CActorMgr::instance()->Find(EActorType_ALogin, 0);
                    if (!pActor)
                    {
                        XERR("[LoginRpc] no local Login actor");
                        result.errCode = 1;
                        return result;
                    }
                    CLuaActor *pLua = dynamic_cast<CLuaActor *>(pActor);
                    if (!pLua)
                    {
                        XERR("[LoginRpc] Login actor is not a CLuaActor");
                        result.errCode = 1;
                        return result;
                    }

                    std::string reply;
                    if (!pLua->DispatchRpcReply("login", reqBody.data(), reqBody.size(), reply))
                    {
                        XERR("[LoginRpc] dispatch to on_rpc_login failed");
                        result.errCode = 1;
                        return result;
                    }

                    result.errCode = 0;
                    result.rspBody = std::move(reply);
                    return result;
                });
        });

    // --- 通用业务转发 (Gate→任意 Actor) ---
    // body = [int32 actor_type][int64 actor_id][客户端 protobuf 负载]
    // 目标 Actor 需实现 Lua on_rpc_forward(payload) → 返回客户端回包 protobuf。
    server->Register(
        RPC_METHOD_FORWARD_ACTOR,
        [](const std::string &reqBody) -> RpcHandlerResult
        {
            RpcHandlerResult result;
            if (reqBody.size() < 12)
            {
                result.errCode = 1;
                return result;
            }

            const uint8 *p = (const uint8 *)reqBody.data();
            int32 type = 0;
            int64 id = 0;
            for (int32 i = 0; i < 4; ++i)
            {
                type |= ((int32)p[i]) << (8 * i);
            }
            for (int32 i = 0; i < 8; ++i)
            {
                id |= ((int64)p[4 + i]) << (8 * i);
            }
            std::string payload = reqBody.substr(12);

            // 转发会触碰进程唯一 lua_State (on_rpc_forward), 必须投递到调度线程串行执行,
            // 不能在本 RPC 服务线程直接调 DispatchRpcReply —— 否则与 Actor 消息循环并发
            // 损坏 Lua 栈 (见 RPC_METHOD_PUSH 同款线程模型说明)。阻塞等待调度线程返回。
            result = CActorScheduler::instance()->DispatchSync(
                [type, id, payload = std::move(payload)]() -> RpcHandlerResult
                {
                    RpcHandlerResult r;
                    CActor *pActor = CActorMgr::instance()->Find((EActorType)type, id);
                    if (!pActor)
                    {
                        // 多实例 Actor (Battle/Spectator) 结算后即被卸载, 客户端在收到
                        // END_NTF 前仍有在途 INPUT_REQ 会在此落空, 属正常清理窗口, 降级
                        // 为警告而非错误。
                        XWRN("[ForwardRpc] no local actor type=%d id=%lld", type, (int64)id);
                        r.errCode = 1;
                        return r;
                    }
                    CLuaActor *pLua = dynamic_cast<CLuaActor *>(pActor);
                    if (!pLua)
                    {
                        XERR("[ForwardRpc] actor type=%d is not a CLuaActor", type);
                        r.errCode = 1;
                        return r;
                    }

                    std::string reply;
                    if (!pLua->DispatchRpcReply("forward", payload.data(), payload.size(), reply))
                    {
                        XERR("[ForwardRpc] dispatch to on_rpc_forward failed type=%d id=%lld", type,
                             (int64)id);
                        r.errCode = 1;
                        return r;
                    }

                    r.errCode = 0;
                    r.rspBody = std::move(reply);
                    return r;
                });
            return result;
        });

    // --- 服务器主动推送 (业务Actor→Gate) ---
    // body = [int64 playerId][客户端 protobuf 负载]
    // 派发给本地 Gate Actor 的 on_rpc_push, 由其按 playerId 反查连接并下发客户端。
    //
    // 线程模型: 本 handler 运行在 RPC 服务线程。若在此直接调用 Lua on_rpc_push,
    // 会与 Actor 调度线程上的 on_msg (客户端消息) 并发触碰进程唯一 lua_State, 造成
    // 栈损坏 (表现为 "got 'table'" / dispatch 失败)。因此改为投递系统消息
    // SYS_RPC_PUSH 到 Gate Actor 的消息队列, 由调度线程串行执行 on_rpc_call("push")
    // (见 CLuaActor::HandleSystemMessage)。推送无回包, 异步投递即可。
    server->Register(RPC_METHOD_PUSH,
                     [](const std::string &reqBody) -> RpcHandlerResult
                     {
                         RpcHandlerResult result;
                         if (reqBody.size() < 8)
                         {
                             result.errCode = 1;
                             return result;
                         }

                         CActor *pActor = CActorMgr::instance()->Find(EActorType_AGate, 0);
                         if (!pActor)
                         {
                             XERR("[PushRpc] no local Gate actor");
                             result.errCode = 1;
                             return result;
                         }

                         pActor->PostMessage(xMsgId::SYS_RPC_PUSH, reqBody);

                         result.errCode = 0;
                         return result;
                     });

    // --- 定向踢下线 (Login→Gate) ---
    // body = [int64 cid] (小端 8 字节)。由 Login 进程在「同账号并发登录踢旧」时
    // 定向发到旧连接的 Gate 进程。直接调 CNetGateway::KickConnection 关闭 socket,
    // 随后 xNetAgent 触发 OnDisconnect → SYS_DISCONNECT → Gate Lua on_rpc_disconnect
    // 完成连接映射清理与离线收尾 (无需经 Lua, 无回包)。
    server->Register(RPC_METHOD_KICK,
                     [](const std::string &reqBody) -> RpcHandlerResult
                     {
                         RpcHandlerResult result;
                         if (reqBody.size() < 8)
                         {
                             result.errCode = 1;
                             return result;
                         }

                         const uint8 *p = (const uint8 *)reqBody.data();
                         int64 cid = 0;
                         for (int32 i = 0; i < 8; ++i)
                         {
                             cid |= ((int64)p[i]) << (8 * i);
                         }

                         XLOG("[KickRpc] kick connection cid=%lld", (int64)cid);
                         CNetGateway::instance()->KickConnection((NetCID)cid);

                         result.errCode = 0;
                         return result;
                     });

    XLOG("[ActorRpc] registered %d actor RPC methods", 7);
}
