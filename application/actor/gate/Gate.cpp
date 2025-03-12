#pragma once

#include "Gate.h"


NetCID OnAccept(void *pParam, tcp_socket& s) {
    CGate* pGate = (CGate*)pParam;



    return pGate->GetActorID();
}


void OnPackageCall(void* pParam, tcp_socket& s, uint64 u64CID, NetPackageSharedPtr pPackage) {
    CGate* pGate = (CGate*)pParam;

    // 消息转发

    std::string sData = "hello world";
    boost::asio::const_buffer asio_buf(sData.c_str(), sData.length());
    std::vector<boost::asio::const_buffer> buffers_sequence;
    buffers_sequence.push_back(asio_buf);
    s.send(buffers_sequence);
}

bool CGate::Init() {
    // 启动网络监听
    NetAgentConfigST stAgentConfig;
    stAgentConfig.strServiceName = "";
    stAgentConfig.strHost = "0.0.0.0";
    stAgentConfig.u16Port = 5261;
    stAgentConfig.pAcceptCall = OnAccept;
    stAgentConfig.pPackageCall = OnPackageCall;
    stAgentConfig.u16CacheBuffSize = 1024;
    stAgentConfig.pParam = this;
    m_pNetAgent = new xNetAgent(stAgentConfig);
    m_pNetAgent->thread_start();
    return true;
}

