#pragma once

#include "xServerDefine.h"
#include "xBase/xSingleton.h"


class CActorRegister : public xSingleton<CActorRegister>
{
public:
    CActorRegister(){}
    virtual ~CActorRegister() {
    }

public:
    bool LoadFromDB(int32 i32Type, int64 i64Id){}
    bool UpdateToDB(){}
    bool RemoveFromDB(){}

public:
    bool LoadFromRedis(){}
    bool UpdateToRedis(){}
    bool RemoveFromRedis(){}

private:

};


