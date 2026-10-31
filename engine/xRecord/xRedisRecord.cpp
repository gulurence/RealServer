/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: xRedisRecord.cpp
      Purpose:   Redis 记录缓存实现

      CRedisRecord 的 Load/Update/Remove 经 CRedisPoolMgr 取连接池客户端执行 Get/Set/Del
      Redis key 格式为 {type}.{id}, 连接池名默认 actor_cache
  *****************************************************************/

#include "xRedisRecord.h"
#include "xRedis/RedisMgr.h"
#include "xLog/xLog.h"

void CRedisRecord::MakeActorKey(const RecordDataST *pData, std::string &strRedisKey)
{
    char szRedisKey[128] = {0};
#ifdef _WIN32
    sprintf(szRedisKey, "%d.%I64d", pData->m_i32Type, pData->m_i64Id);
#else
    sprintf(szRedisKey, "%d.%ld", pData->m_i32Type, pData->m_i64Id);
#endif
    strRedisKey = szRedisKey;
}

bool CRedisRecord::Load(RecordDataST *pData)
{
    std::string strRedisKey;
    MakeActorKey(pData, strRedisKey);
    auto *pConnect = CRedisPoolMgr::getMe().GetRedisCli(m_strPoolTitle);
    if (!pConnect)
    {
        XWRN("[CRedisRecord] Load: no Redis client for pool '%s'", m_strPoolTitle.c_str());
        return false;
    }
    return pConnect->Get(strRedisKey.c_str(), pData) == REDIS_OK;
}

bool CRedisRecord::Update(const RecordDataST *pData)
{
    std::string strRedisKey;
    MakeActorKey(pData, strRedisKey);
    auto *pConnect = CRedisPoolMgr::getMe().GetRedisCli(m_strPoolTitle);
    if (!pConnect)
    {
        XWRN("[CRedisRecord] Update: no Redis client for pool '%s'", m_strPoolTitle.c_str());
        return false;
    }
    return pConnect->Set(strRedisKey.c_str(), pData) == REDIS_OK;
}

bool CRedisRecord::Remove(const RecordDataST *pData)
{
    std::string strRedisKey;
    MakeActorKey(pData, strRedisKey);
    auto *pConnect = CRedisPoolMgr::getMe().GetRedisCli(m_strPoolTitle);
    if (!pConnect)
    {
        XWRN("[CRedisRecord] Remove: no Redis client for pool '%s'", m_strPoolTitle.c_str());
        return false;
    }
    return pConnect->Del(strRedisKey.c_str()) == REDIS_OK;
}
