#pragma once

#include "xNetDefine.h"
#include "xCircularBuffer.h"
#include "xBase/xThread.h"

class xNetAgent : public xThread
{
public:
    xNetAgent(const NetAgentConfigST& stConfig);
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

