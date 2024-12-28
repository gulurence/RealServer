#include "xRpc.h"


void SRpcService::Start(uint16 u16Port) {
    m_u16Port = u16Port;
    char pszListenAddress[128] = { 0 };
    sprintf_s(pszListenAddress, 100, "localhost:%d", m_u16Port);
    // 设置服务器监听地址（例如 localhost:50051）
    builder.AddListeningPort(pszListenAddress, grpc::InsecureServerCredentials());

    // 构建并启动服务器
    m_pServicePtr = builder.BuildAndStart();

    std::cout << "Server listening on localhost:50051" << std::endl;
}

void SRpcService::thread_proc() {
    while (m_bRunning) {
        m_pServicePtr->Wait();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void SRpcService::RegistService(grpc::Service* pService) {
    builder.RegisterService(pService);
}

std::shared_ptr<grpc::Channel> CRpcService::GetChannel(const std::string& strServerName) {
    auto it = m_mapChannel.find(strServerName);
    if (it != m_mapChannel.end()) {
        return it->second;
    }
    return nullptr;
}

bool CRpcService::ConnectToServer(const std::string& strServerName, uint16 u16Port) {
    if (strServerName.empty()) {
        return false;
    }
    char pszListenAddress[128] = { 0 };
    sprintf_s(pszListenAddress, 100, "localhost:%d", u16Port);
    auto channelPtr = grpc::CreateChannel(pszListenAddress, grpc::InsecureChannelCredentials());
    if (channelPtr) {
        m_mapChannel[strServerName] = channelPtr;
    } else {
        return false;
    }
    return true;
}


