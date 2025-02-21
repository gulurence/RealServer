#pragma once

#include "Actor.h"
#include "xBase/xSingleton.h"


class CActorMgr : public xSingleton<CActorMgr>
{
public:
    CActorMgr();
    ~CActorMgr();

private:


public:
    bool Init() {


        return true;
    }

public:
    xErrorCode Install(const ActorID &i64ActorId) {





    }

    CActor* UnInstall() {

    }
};

