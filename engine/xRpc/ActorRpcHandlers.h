/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: ActorRpcHandlers.h
      Purpose:   Actor RPC 处理器注册声明

      声明 RegisterActorRpcHandlers(), 在 CRpcTcpServer::Start 之前完成跨进程 Actor 管理方法注册
  *****************************************************************/

#pragma once

// =============================================================================
// ActorRpcHandlers — 把跨进程 Actor 管理 (CreateActor/FindActor/UninstallActor)
// 从 gRPC (ActorMgrServiceImpl) 迁移到自建 asio-TCP RPC method。
//
// 在 CRpcTcpServer::Start 之前调用 RegisterActorRpcHandlers() 完成注册。
// 请求/响应仍复用 actor_mgr.proto 的消息结构 (actormgr::*), 不再走 gRPC stub。
// =============================================================================

void RegisterActorRpcHandlers();
