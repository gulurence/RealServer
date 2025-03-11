
#include "xRedisRecord.h"
#include "xRedis/RedisMgr.h"


void CRedisRecord::MakeActorKey(const RecordDataST* pData, std::string& strRedisKey) {
    char szRedisKey[128] = { 0 };
#ifdef _WIN32
    sprintf(szRedisKey, "%d.%I64d", pData->m_i32Type, pData->m_i64Id);
#else
    sprintf(szRedisKey, "%d.%ld", pData->m_i32Type, pData->m_i64Id);
#endif
    strRedisKey = szRedisKey;
}

bool CRedisRecord::Load(RecordDataST* pData) {
    std::string strRedisKey;
    MakeActorKey(pData, strRedisKey);
    auto *pConnect = CRedisPoolMgr::getMe().GetConnection();
    if (!pConnect) {
        return false;
    }
    return pConnect->Get(strRedisKey.c_str(), pData) == REDIS_OK;
}

bool CRedisRecord::Update(const RecordDataST* pData) {
    std::string strRedisKey;
    MakeActorKey(pData, strRedisKey);
    auto* pConnect = CRedisPoolMgr::getMe().GetConnection();
    if (!pConnect) {
        return false;
    }
    return pConnect->Set(strRedisKey.c_str(), pData) == REDIS_OK;
}

bool CRedisRecord::Remove(const RecordDataST* pData) {
    std::string strRedisKey;
    MakeActorKey(pData, strRedisKey);
    auto* pConnect = CRedisPoolMgr::getMe().GetConnection();
    if (!pConnect) {
        return false;
    }
    return pConnect->Del(strRedisKey.c_str()) == REDIS_OK;
}
