#include "xRpc.h"
#include "xLog/xLog.h"

// =============================================================================
// xRpc.cpp — RpcCallMgr 遗留工作线程池实现
//
// gRPC 版 RPC (SRpcService / CRpcService) 已退役, 详见 xRpc.h 顶部说明。
// 本文件仅保留 RpcCallMgr (供 xNats/xService 遗留宏使用)。
// =============================================================================

void RpcCallProcess(RpcCallMgr *pRpcCallMgr)
{
    while (pRpcCallMgr->m_isRunning)
    {
        auto pCall = pRpcCallMgr->PopCall();
        if (pCall)
        {
            pCall();
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::nanoseconds(1));
        }
    }
}

void RpcCallMgr::AddCall(OnEventRpcCallBack pCall)
{
    m_poolEvent.Push(pCall);
}

void RpcCallMgr::Init()
{
    XLOG("[INIT] RpcCallMgr starting legacy work thread");
    std::thread t(RpcCallProcess, this);
    t.detach();
    XLOG("[INIT] RpcCallMgr legacy work thread started");
}

OnEventRpcCallBack RpcCallMgr::PopCall()
{
    OnEventRpcCallBack pOut = nullptr;
    m_poolEvent.Pop(pOut);
    return pOut;
}
