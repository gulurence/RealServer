#pragma once

#include "xRecordDefine.h"
#include "xBase/xSingleton.h"
#include "xBase/xObjPool.h"
#include "xRedisRecord.h"
#include "xDatabaseRecord.h"


// CRecordMgr — unifying data persistence manager.
//
// Flow for each operation:
//   LoadActor:  Redis GET → (miss) DB SELECT → Redis SET (cache)
//   UpdateActor: Redis SET (sync) → DB INSERT/UPDATE (async)
//   RemoveActor: Redis DEL (sync) → DB DELETE (async)
//
// The Redis layer acts as the synchronous cache; the DB layer is
// asynchronous (queued via COperatorMgr / DBOperator threads).
class CRecordMgr : public xSingleton<CRecordMgr>
{
public:
    CRecordMgr()
        : m_xRedisRecord("actor_cache")
        , m_xDatabaseRecord("actor_db") {
    }
    virtual ~CRecordMgr() {
    }

public:
    // Load actor data: Redis first, DB fallback, cache on DB hit.
    // pData->m_i32Type and m_i64Id must be set before calling.
    // On return, pData contains the binary record (or empty for new actors).
    bool LoadActor(RecordDataST* pData);

    // Update actor data: Redis sync write + enqueue DB async write.
    bool UpdateActor(const RecordDataST* pData);

    // Remove actor data: Redis sync delete + enqueue DB async delete.
    bool RemoveActor(const RecordDataST* pData);

public:
    void PushBackRecordData(RecordDataST* pData) {
        m_xPool.DeleteObj(pData);
    }

private:
    xObjPool<RecordDataST> m_xPool;
    CRedisRecord    m_xRedisRecord;
    CDatabaseRecord m_xDatabaseRecord;
}; 
