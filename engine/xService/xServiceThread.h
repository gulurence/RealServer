#pragma once

#include "xEvent/xEvent.h"
#include "xBase/xCircularPool.h"


class xServiceThread : public xThread
{
public:
    xServiceThread(uint16 u16ThreadIndex);
    virtual ~xServiceThread();

public:


public:
    virtual void thread_proc();

private:
    uint16 m_u16ThreadIndex;
};

typedef std::list<xServiceThread*> ServiceThreadList;



class xServiceThreadMgr
{
public:
    xServiceThreadMgr(uint16 u16ThreadCount = 4);
    ~xServiceThreadMgr();

private:
    void Start();
    void Pause();
    void Stop();

private:
    uint16 m_u16ThreadCount;
    ServiceThreadList m_listServiceThread;
};


