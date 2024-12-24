#pragma once

#include "../xNet/xNetProcessor.h"
#include "../xBase/xObjPool.h"

class xTask : public xNetProcessor, public xObjPoolBase
{
public:
    xTask();
    virtual ~xTask();

public:
    void init(int fd);
    void cleanup();

    virtual bool isClient(){return false;}
    virtual bool isTask(){return true;}
};



