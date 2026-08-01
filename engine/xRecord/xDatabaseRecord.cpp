#include "xDatabaseRecord.h"
#include "xDatabase/xOperatorMgr.h"
#include "xLog/xLog.h"

// ---------------------------------------------------------------------------
// Build a simple column descriptor for the generic actor data table.
// The DB schema is expected to store: id (INT64) + data (BIN blob) per table.
// Each actor type maps to its own MySQL table named after the actor.
// ---------------------------------------------------------------------------
static dbCol s_actorDbCols[] = {
    { "id",   DBDATA_TYPE_UInt64, 8 },
    { "data", DBDATA_TYPE_BIN,    0 },   // 0 = variable-length binary
    { NULL,   0,                  0 }    // terminator
};

bool CDatabaseRecord::Load(RecordDataST* pData) {
    if (!pData) {
        XERR("[CDatabaseRecord] Load: null pData");
        return false;
    }

    auto* pOperatorNode = COperatorMgr::getMe().PopOperatorNode();
    if (!pOperatorNode) {
        XWRN("[CDatabaseRecord] Load: no available operator node for pool '%s'",
             m_strPoolTitle.c_str());
        return false;
    }

    // Fill the operator node for a SELECT operation
    pOperatorNode->m_stTitle     = m_strPoolTitle;
    pOperatorNode->m_enOpType    = ODOT_SELECT;
    pOperatorNode->m_pCol        = s_actorDbCols;

    // Build a simple "id = <value>" WHERE clause
    char szWhere[64] = { 0 };
#ifdef _WIN32
    sprintf(szWhere, "id=%I64d", pData->m_i64Id);
#else
    sprintf(szWhere, "id=%ld", pData->m_i64Id);
#endif
    pOperatorNode->m_strWhere    = szWhere;

    // Table name derived from actor type (subclasses can override via a mapping)
    char szTable[64] = { 0 };
    sprintf(szTable, "actor_%d", pData->m_i32Type);
    pOperatorNode->m_strTableName = szTable;

    COperatorMgr::getMe().PushOperator(pOperatorNode);
    return true;
}

bool CDatabaseRecord::Update(const RecordDataST* pData) {
    if (!pData) {
        XERR("[CDatabaseRecord] Update: null pData");
        return false;
    }

    auto* pOperatorNode = COperatorMgr::getMe().PopOperatorNode();
    if (!pOperatorNode) {
        XWRN("[CDatabaseRecord] Update: no available operator node for pool '%s'",
             m_strPoolTitle.c_str());
        return false;
    }

    // Fill the operator node for an INSERT operation (upsert handled by DB logic)
    pOperatorNode->m_stTitle     = m_strPoolTitle;
    pOperatorNode->m_enOpType    = ODOT_INSERT;
    pOperatorNode->m_pCol        = s_actorDbCols;

    // Copy the binary record data
    pOperatorNode->m_stData.Init(
        reinterpret_cast<unsigned char*>(pData->Data()),
        static_cast<unsigned short>(pData->Size()));

    char szTable[64] = { 0 };
    sprintf(szTable, "actor_%d", pData->m_i32Type);
    pOperatorNode->m_strTableName = szTable;

    COperatorMgr::getMe().PushOperator(pOperatorNode);
    return true;
}

bool CDatabaseRecord::Remove(const RecordDataST* pData) {
    if (!pData) {
        XERR("[CDatabaseRecord] Remove: null pData");
        return false;
    }

    auto* pOperatorNode = COperatorMgr::getMe().PopOperatorNode();
    if (!pOperatorNode) {
        XWRN("[CDatabaseRecord] Remove: no available operator node for pool '%s'",
             m_strPoolTitle.c_str());
        return false;
    }

    // Fill the operator node for a DELETE operation
    pOperatorNode->m_stTitle     = m_strPoolTitle;
    pOperatorNode->m_enOpType    = ODOT_DELETE;

    char szWhere[64] = { 0 };
#ifdef _WIN32
    sprintf(szWhere, "id=%I64d", pData->m_i64Id);
#else
    sprintf(szWhere, "id=%ld", pData->m_i64Id);
#endif
    pOperatorNode->m_strWhere    = szWhere;

    char szTable[64] = { 0 };
    sprintf(szTable, "actor_%d", pData->m_i32Type);
    pOperatorNode->m_strTableName = szTable;

    COperatorMgr::getMe().PushOperator(pOperatorNode);
    return true;
}
