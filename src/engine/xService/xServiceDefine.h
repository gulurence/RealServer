#pragma once

#include "xNet/xNetDefine.h"

enum xServiceState
{
    XSERVICE_CREATE = 0,
    XSERVICE_INIT,        //connect other SERVICE
    XSERVICE_RUN,         //all SERVICE connected, do work
    XSERVICE_SAVE,        //
    XSERVICE_STOP,        //will SERVICE_stop
    XSERVICE_FINISH,      //all done
};



























