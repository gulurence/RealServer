/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: xNetAgent.h
      Purpose:   asio-TCP 网络代理接口：基于协程的 TCP 监听与连接处理。

      声明 xNetAgent 类（继承 xThread）：持有 NetAgentConfigST 配置与回调集
      Accept/Listener 为 asio 协程（awaitable），thread_proc 运行 io_context 事件循环
      回调含 pAcceptCall/pPackageCall/pDisconnect/pSocketReady，由上层网关 CNetGateway 等绑定
  *****************************************************************/

#pragma once

#include "xNetDefine.h"
#include "xCircularBuffer.h"
#include "xBase/xThread.h"

class xNetAgent : public xThread
{
public:
    xNetAgent(const NetAgentConfigST &stConfig);
    virtual ~xNetAgent();

public:
    virtual void thread_proc();

protected:
    virtual void Release();

protected:
    awaitable<void> Accept(tcp_socket stSocket);
    awaitable<void> Listener(uint16 u16Port);

private:
    NetAgentConfigST m_stConfig;
};
