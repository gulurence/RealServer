
#include "xDatabaseRecord.h"
#include "xDatabase/xOperatorMgr.h"


bool CDatabaseRecord::Load(RecordDataST* pData) {
    auto *pOperatorNode = COperatorMgr::getMe().PopOperatorNode();
    if (!pOperatorNode) {
        XWRN("CDatabaseRecord::Load not find database pool [title:%s] !!!","");
        return false;
    }

    pOperatorNode->m_pCol;


    pOperatorNode->m_stTitle;
    pOperatorNode->m_enOpType = ODOT_NULL;
    pOperatorNode->m_strTableName;
    pOperatorNode->m_strWhere;
    pOperatorNode->m_pCol = nullptr;
    pOperatorNode->m_stData;


    COperatorMgr::getMe().PushOperator(pOperatorNode);
    return true;
}

bool CDatabaseRecord::Update(const RecordDataST* pData) {

    return true;
}

bool CDatabaseRecord::Remove(const RecordDataST* pData) {

    return true;
}

