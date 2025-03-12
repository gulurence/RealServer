#pragma once

#include "xService/xService.h"

#include "xNet/xNetAgent.h"





class CGateClient 
{
public:
    CGateClient(ActorID i64CID, tcp_socket& stSocket) :m_stSocket(stSocket){}
    ~CGateClient() {}

private:
    tcp_socket& m_stSocket;

public:
    void SendMsg() {

    }

public:
    virtual bool Init() {


        return true;
    }

private:
    xNetAgent* m_pNetAgent = nullptr;
};

typedef std::unordered_map<NetCID, CGateClient*> GateClientMap;

