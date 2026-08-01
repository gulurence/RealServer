#include "xRpc.h"
#include "xLog/xLog.h"

void SRpcService::Start(uint16 u16Port) {
    m_u16Port = u16Port;
    char pszListenAddress[128] = { 0 };
    snprintf(pszListenAddress, 100, "localhost:%d", m_u16Port);
    XLOG("[INIT] gRPC server binding to %s", pszListenAddress);

    builder.AddListeningPort(pszListenAddress, grpc::InsecureServerCredentials());

    // 构建并启动服务器
    m_pServicePtr = builder.BuildAndStart();
    if (m_pServicePtr) {
        XLOG("[INIT] gRPC server started on port %u", m_u16Port);
    } else {
        XERR("[INIT] gRPC server failed to start on port %u", m_u16Port);
        return;
    }

    // 启动线程
    thread_start();
}

void SRpcService::thread_proc() {
    while (m_bRunning) {
        m_pServicePtr->Wait();
        //std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void SRpcService::RegistService(grpc::Service* pService) {
    builder.RegisterService(pService);
    XLOG("[INIT] gRPC service registered");
}





std::shared_ptr<grpc::Channel> CRpcService::GetChannel(const std::string& strServerName) {
    int32 index = 0;
    auto it = m_mapChannelSize.find(strServerName);
    if (it != m_mapChannelSize.end()) {
        it->second.second++;
        it->second.second = it->second.second % it->second.first;
        index = it->second.second;
    } else {
        return nullptr;
    }

    auto it_channel = m_mapChannel.find(strServerName);
    if (it_channel != m_mapChannel.end()) {
        return it_channel->second[index];
    }

    return nullptr;
}

bool CRpcService::ConnectToServer(const std::string& strServerName, int32 i32PoolCount, const std::string &strIp, uint16 u16Port) {
    if (strServerName.empty()) {
        return false;
    }

    m_mapChannelSize[strServerName] = std::make_pair(i32PoolCount,0);

    char pszListenAddress[128] = { 0 };
    snprintf(pszListenAddress, 100, "%s:%d", strIp.c_str(), u16Port);
    XLOG("[INIT] RPC client connecting to %s (%s) x%d channels",
         strServerName.c_str(), pszListenAddress, i32PoolCount);

    for (int i = 0;i< i32PoolCount;++i) {
        auto channelPtr = grpc::CreateChannel(pszListenAddress, grpc::InsecureChannelCredentials());
        if (channelPtr) {
            m_mapChannel[strServerName].push_back(channelPtr);
        }
    }

    XLOG("[INIT] RPC client '%s' ready: %zu channels",
         strServerName.c_str(), m_mapChannel[strServerName].size());
    return true;
}

bool CRpcService::ConnectToLocalServer(const std::string& strServerName, int32 i32PoolCount, uint16 u16Port) {
    if (strServerName.empty()) {
        return false;
    }

    m_mapChannelSize[strServerName] = std::make_pair(i32PoolCount, 0);

    char pszListenAddress[128] = { 0 };
    snprintf(pszListenAddress, 100, "localhost:%d", u16Port);
    XLOG("[INIT] RPC client connecting to %s (%s) x%d channels",
         strServerName.c_str(), pszListenAddress, i32PoolCount);

    for (int i = 0; i < i32PoolCount; ++i) {
        auto channelPtr = grpc::CreateChannel(pszListenAddress, grpc::InsecureChannelCredentials());
        if (channelPtr) {
            m_mapChannel[strServerName].push_back(channelPtr);
        }
    }
    XLOG("[INIT] RPC client '%s' ready: %zu channels",
         strServerName.c_str(), m_mapChannel[strServerName].size());
    return true;
}

void RpcCallProcess(RpcCallMgr *pRpcCallMgr) {
    while (pRpcCallMgr->m_isRunning) {
        auto pCall = pRpcCallMgr->PopCall();
        if (pCall) {
            pRpcCallMgr->RpcCall(pCall);
        } 
        else {
          std::this_thread::sleep_for(std::chrono::nanoseconds(1));
        }
    }
}

void RpcCallMgr::AddCall(OnEventRpcCallBack pCall) {
    m_poolEvent.Push(pCall);
}

void RpcCallMgr::Init() {
    XLOG("[INIT] RpcCallMgr starting async RPC thread");
    std::thread t(RpcCallProcess, this);
    t.detach();
    XLOG("[INIT] RpcCallMgr async RPC thread started");
}

OnEventRpcCallBack RpcCallMgr::PopCall() {
    OnEventRpcCallBack pOut = nullptr;
    m_poolEvent.Pop(pOut);
    return pOut;
}

