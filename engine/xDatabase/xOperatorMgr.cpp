#include "xOperatorMgr.h"


DatabaseOpNode* COperatorMgr::PopOperatorNode() {
    auto* pNode = m_pDatabaseOpNodePool.NewObj();
    return pNode;
}

void COperatorMgr::PushOperatorNode(DatabaseOpNode* pNode) {
    m_pDatabaseOpNodePool.DeleteObj(pNode);
}

bool COperatorMgr::ConnectToDB(const DBConfigST& stConfig) {
    auto it = m_mapDBOperator.find(stConfig.title);
    if (it != m_mapDBOperator.end()) {
        XWRN("COperatorMgr::ConnectToDB have same connect pool [title:%s] !!!", stConfig.title.c_str());
        return false;
    }
    auto* pConnect = new DBOperator();
    if (!pConnect->Init(stConfig.host.c_str(), stConfig.port, stConfig.name.c_str(), stConfig.user.c_str(), stConfig.passwd.c_str(), stConfig.poolCount)) {
        XWRN("COperatorMgr::ConnectToDB pConnect->init error [title:%s,host:%s,port:%d,name:%s,user:%s,passwd:%s,poolCount:%d] !!!", stConfig.title.c_str(), stConfig.host.c_str(), stConfig.port, stConfig.name.c_str(), stConfig.user.c_str(), stConfig.passwd.c_str(), stConfig.poolCount);
        return false;
    }
    m_mapDBOperator[stConfig.title] = pConnect;
    return true;
}

bool COperatorMgr::PushOperator(DatabaseOpNode* pNode) {
    if (pNode->m_stTitle.empty()) {
        XWRN("COperatorMgr::PushOperator pNode->m_stTitle.empty() !!!");
        return false;
    }
    auto it = m_mapDBOperator.find(pNode->m_stTitle);
    if (it == m_mapDBOperator.end()) {
        return false;
    }
    auto* pDBOperator = it->second;
    pDBOperator->PushOperator(pNode);
    return true;
}

