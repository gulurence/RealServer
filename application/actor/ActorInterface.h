#pragma once

#include "xService/xService.h"


class CActorInterface
{
public:
    CActorInterface(){}
    ~CActorInterface(){}

private:



public:
    virtual int32 Install() = 0;
    virtual int32 UnInstall() = 0;
};

