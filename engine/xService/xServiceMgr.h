#pragma once

#include "xServiceDefine.h"

#include "xService.h"

#include "xBase/xSingleton.h"




class xServiceMgr : public xSingleton<xServiceMgr>
{
public:
    xServiceMgr();
    virtual ~xServiceMgr();

public:
    bool Regist(xService *);
    xService* Get(const ServiceID& u64ServiceID);
    bool UnRegist();

public:
    ServiceMap& GetServices() {
        return m_mapService;
    }

public:
    virtual void Release();

    std::mutex m_stLock;
private:
    ServiceMap m_mapService;
};



