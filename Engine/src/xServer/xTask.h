#pragma once
#include "xNetProcessor.h"

class xTask : public xNetProcessor
{
public:
    xTask(int fd);
    virtual ~xTask();

    virtual bool isClient(){return false;}
    virtual bool isTask(){return true;}
};
