#pragma once

// =============================================================================
// ActorRpcHandlers — 把跨进程 Actor 管理 (CreateActor/FindActor/UninstallActor)
// 从 gRPC (ActorMgrServiceImpl) 迁移到自建 asio-TCP RPC method。
//
// 在 CRpcTcpServer::Start 之前调用 RegisterActorRpcHandlers() 完成注册。
// 请求/响应仍复用 actor_mgr.proto 的消息结构 (actormgr::*), 不再走 gRPC stub。
// =============================================================================

void RegisterActorRpcHandlers();
