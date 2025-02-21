#pragma once

#include "RedisCli.h"
#include "xBase/xSingleton.h"


class CRedisMgr : public xSingleton<CRedisMgr>
{
public:
    CRedisMgr();
    ~CRedisMgr();

public:
    bool Init(const char* hostName, const int &port, const char* auth, const int &dbIndex, int32 i32PoolCount);
    void Release();

public:
    CRedisCli* PopRedisCli();
    void PushRedisCli(CRedisCli*);

private:
    int32 m_i32PoolCount = 0;
    RedisCliList m_listRedisCli;
    RedisCliList m_listRedisCliFree;
};

