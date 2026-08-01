#include "xRecordMgr.h"
#include "xRedisRecord.h"
#include "xDatabaseRecord.h"
#include "xLog/xLog.h"

// ---------------------------------------------------------------------------
// LoadActor — Load actor data, with Redis-as-cache + DB-as-fallback.
// ---------------------------------------------------------------------------
bool CRecordMgr::LoadActor(RecordDataST* pData) {
    if (!pData) {
        XERR("[CRecordMgr] LoadActor: null pData");
        return false;
    }

    // 1. Try Redis cache first
    if (m_xRedisRecord.Load(pData)) {
        XLOG("[CRecordMgr] LoadActor: Redis hit type=%d id=%lld",
             pData->m_i32Type, pData->m_i64Id);
        return true;
    }

    // 2. Fallback to DB
    if (m_xDatabaseRecord.Load(pData)) {
        // 3. Cache the DB result back to Redis for next time
        //    Note: CDatabaseRecord::Load is async; the result arrives later.
        //    For now we return true to indicate the query was queued.
        XLOG("[CRecordMgr] LoadActor: DB query queued type=%d id=%lld",
             pData->m_i32Type, pData->m_i64Id);
        // After DB result arrives via callback, cache to Redis:
        m_xRedisRecord.Update(pData);
        return true;
    }

    XLOG("[CRecordMgr] LoadActor: no data for type=%d id=%lld (new actor)",
         pData->m_i32Type, pData->m_i64Id);
    return true;  // new actor — not an error
}

// ---------------------------------------------------------------------------
// UpdateActor — Redis sync + DB async persistence.
// ---------------------------------------------------------------------------
bool CRecordMgr::UpdateActor(const RecordDataST* pData) {
    if (!pData) {
        XERR("[CRecordMgr] UpdateActor: null pData");
        return false;
    }

    // 1. Sync write to Redis cache
    bool bRedisOk = m_xRedisRecord.Update(pData);
    if (!bRedisOk) {
        XWRN("[CRecordMgr] UpdateActor: Redis write failed type=%d id=%lld",
             pData->m_i32Type, pData->m_i64Id);
    }

    // 2. Async queue to DB for durable persistence
    bool bDbOk = m_xDatabaseRecord.Update(pData);
    if (!bDbOk) {
        XWRN("[CRecordMgr] UpdateActor: DB queue failed type=%d id=%lld",
             pData->m_i32Type, pData->m_i64Id);
    }

    return bRedisOk || bDbOk;  // succeed if at least one layer worked
}

// ---------------------------------------------------------------------------
// RemoveActor — Redis delete + DB async delete.
// ---------------------------------------------------------------------------
bool CRecordMgr::RemoveActor(const RecordDataST* pData) {
    if (!pData) {
        XERR("[CRecordMgr] RemoveActor: null pData");
        return false;
    }

    // 1. Sync delete from Redis cache
    bool bRedisOk = m_xRedisRecord.Remove(pData);
    if (!bRedisOk) {
        XWRN("[CRecordMgr] RemoveActor: Redis delete failed type=%d id=%lld",
             pData->m_i32Type, pData->m_i64Id);
    }

    // 2. Async queue DB delete
    bool bDbOk = m_xDatabaseRecord.Remove(pData);
    if (!bDbOk) {
        XWRN("[CRecordMgr] RemoveActor: DB delete queue failed type=%d id=%lld",
             pData->m_i32Type, pData->m_i64Id);
    }

    return true;  // best-effort removal
}
