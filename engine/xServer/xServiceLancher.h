#pragma once

#include "xServerDefine.h"


class CServiceLancher
{
public:
    CServiceLancher(){}
    virtual ~CServiceLancher() {
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









