#pragma once

#include "xServiceDefine.h"

#include "xBase/xDefine.h"
#include "xBase/xThread.h"
#include "xBase/xTime.h"
#include "xNet/xSocket.h"


class xServiceMgr : public xThread
{
public:
    xServiceMgr(const char* pszServiceName, uint16 u16Port);
    virtual ~xServiceMgr();


public:
    void ServiceStop() {
        SetServiceState(XSERVICE_STOP);
    }
    xServiceState GetServiceState() { 
        return m_enServiceState; 
    }
    void SetServiceState(xServiceState enState) {
        m_enServiceState = enState;
    }

public:
    virtual void thread_proc();

protected:
    virtual void Release();

private:
    xServiceState m_enServiceState;
    char m_pszServiceName[MAX_NAMESIZE] = { 0 };
    uint16 m_u16Port = 0;
};
