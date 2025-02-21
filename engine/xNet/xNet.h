#pragma once

#include "xNetDefine.h"
#include "xCircularBuffer.h"
#include "xBase/xThread.h"

class xNet : public xThread
{
public:
    xNet(const char* pszServiceName, uint16 u16Port, NetPackageAcceptPtr pAcceptCall, NetPackageCallPtr pPackageCall, uint16 u16CacheBuffSize=1024);
    virtual ~xNet();

public:
    virtual void thread_proc();

protected:
    virtual void Release();

protected:
    awaitable<void> Accept(tcp_socket stSocket);
    awaitable<void> Listener(uint16 u16Port);

private:
    NetPackageAcceptPtr m_pAcceptCall;
    NetPackageCallPtr m_pPackageCall;

private:
    uint16 m_u16CacheBuffSize = 0;
    char m_pszServiceName[MAX_NAMESIZE] = { 0 };
    uint8 m_pszIp[IP_SIZE] = { 0 };
    uint16 m_u16Port = 0;
};

