/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: xRedisRecord.h
      Purpose:   Redis 记录缓存声明

      CRedisRecord 封装 Load/Update/Remove 三步同步缓存操作, 连接池名默认 actor_cache
      内部 MakeActorKey 拼接 {type}.{id} 作为 Redis key
  *****************************************************************/

#pragma once

#include "xRecordDefine.h"

class CRedisRecord
{
public:
    // strPoolTitle must match a pool registered with CRedisPoolMgr::ConnectToRedis().
    // Default "actor_cache" matches the default server.xml entry.
    CRedisRecord(const std::string &strPoolTitle = "actor_cache") : m_strPoolTitle(strPoolTitle)
    {
    }
    virtual ~CRedisRecord()
    {
    }

private:
    void MakeActorKey(const RecordDataST *pData, std::string &strRedisKey);

public:
    bool Load(RecordDataST *pData);
    bool Update(const RecordDataST *pData);
    bool Remove(const RecordDataST *pData);

private:
    std::string m_strPoolTitle;
};
