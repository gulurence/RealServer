#pragma once

#include "xEvent/xEvent.h"
#include "xServiceDefine.h"
#include "xBase/xCircularPool.h"
#include "xScheduler/xScheduler.h"

class xService
{
public:
    xService(const ServiceID &u64ServiceID, const char* pszServiceName);
    virtual ~xService();

public:
    ServiceID Id() { return m_u64ServiceID; }

public:
    virtual xService* Create() = 0;
    virtual bool Init() = 0;
    virtual void Release() = 0;

public:
    xCircularPool<xEvent*>& GetEventCircularPool() {
        return m_stEventCircularPool;
    }

public:
    ServiceScheduler* GetServiceScheduler() {
        return m_ptrServiceScheduler;
    }


private:
    xCircularPool<xEvent*> m_stEventCircularPool;

private:
    ServiceScheduler* m_ptrServiceScheduler;

private:
    ServiceID m_u64ServiceID;
    char m_pszServiceName[MAX_NAMESIZE] = { 0 };

};

typedef std::unordered_map<ServiceID, xService*> ServiceMap;





class ServiceLogin : public xService
{
public:
    ServiceLogin(const ServiceID& u64ServiceID, const char* pszServiceName):xService(u64ServiceID, pszServiceName){}
    ~ServiceLogin(){}

private:

public:
    virtual xService* Create() { return nullptr; }
    virtual bool Init() { return true; }
    virtual void Release() { return; }

public:

};


