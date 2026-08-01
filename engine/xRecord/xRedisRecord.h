#pragma once

#include "xRecordDefine.h"


class CRedisRecord
{
public:
    // strPoolTitle must match a pool registered with CRedisPoolMgr::ConnectToRedis().
    // Default "actor_cache" matches the default server.xml entry.
    CRedisRecord(const std::string& strPoolTitle = "actor_cache")
        : m_strPoolTitle(strPoolTitle) {}
    virtual ~CRedisRecord() {
    }

private:
    void MakeActorKey(const RecordDataST* pData, std::string &strRedisKey);

public:
    bool Load(RecordDataST *pData);
    bool Update(const RecordDataST* pData);
    bool Remove(const RecordDataST* pData);

private:
    std::string m_strPoolTitle;
};
