#include "xOperatorNode.h"

#include "xOperatorMgr.h"


DatabaseOpNode::~DatabaseOpNode() {
}

bool DatabaseOpNode::OnCallLogic() {
    if (!m_pDBConnPool) {
        return false;
    }

    bool bRet = false;
    switch (m_enOpType)
    {
    case ODOT_INSERT: { bRet = onInsert(); }break;
    case ODOT_SELECT: { bRet = onSelect(); }break;
    case ODOT_UPDATE: { bRet = onUpdate(); }break;
    case ODOT_DELETE: { bRet = onDelete(); }break;
    default:return false;
    }
    XERR("DatabaseOpNode::OnCallLogic m_enOpType :%d", m_enOpType);

    // 异步回调
    if (m_pDBOpCallBack) {
        m_pDBOpCallBack(bRet);
    }

    return bRet;
}

bool DatabaseOpNode::onInsert() {
    if (!m_pDBConnPool) {
        XERR("DatabaseOpNode::onInsert - !m_pDBConnPool");
        return false;
    }

    if (!m_pCol) {
        XERR("DatabaseOpNode::onInsert !m_pCol tablename:%s", m_strTableName.c_str());
        return false;
    }
    DBConnID connID = m_pDBConnPool->getDBConn();
    if ((DBConnID)-1 != connID) {
        uint64 ret = m_pDBConnPool->exeInsert(connID, m_strTableName.c_str(), (const dbCol *)m_pCol, (const uint8*)m_stData.mBuffer);
        m_pDBConnPool->putDBConn(connID);
        if (UInt64_MAX != ret) {
            //XLOG("[保存数据],DatabaseOpNode::onInsert - tablename:%s,success", m_strTableName.c_str());
            return true;
        }
    }

    XERR("DatabaseOpNode::onInsert exeInsert error tablename:%s", m_strTableName.c_str());
    return false;
}

bool DatabaseOpNode::onSelect() {
    //XLOG("DatabaseOpNode::onSelect");

    if (!m_pDBConnPool) {
        XERR("DatabaseOpNode::onSelect - !m_pDBConnPool");
        return false;
    }

    m_listSeleteData.clear();
    if (!m_pCol) {
        XERR("DatabaseOpNode::onSelect !m_pCol tablename:%s", m_strTableName.c_str());
        return false;
    }

    DBConnID connID = m_pDBConnPool->getDBConn();
    if ((DBConnID)-1 != connID) {
        unsigned char *result = nullptr;
        uint64 num = m_pDBConnPool->exeSelect(connID, m_strTableName.c_str(), (const dbCol *)m_pCol, (unsigned char**)&result, m_strWhere.c_str());
        m_pDBConnPool->putDBConn(connID);
        if (UInt64_MAX == num) {
            XERR("DatabaseOpNode::onSelect select error tablename:%s,where:%s", m_strTableName.c_str(), m_strWhere.c_str());
            return 0;
        } else {
            //XERR("DatabaseOpNode::onSelect !m_pCol tablename:%s success count:%d", m_strTableName.c_str(), num);
            for (int i = 0; i < num; ++i) {
                DBDataNode stData;
                stData.Init(&result[m_stData.mLen*i], m_stData.mLen);
                m_listSeleteData.push_back(stData);
            }

            SAFE_DELETE_VEC(result);
            return true;
        }
    }

    XERR("DatabaseOpNode::onSelect !m_pCol tablename:%s fail", m_strTableName.c_str());
    return false;
}

bool DatabaseOpNode::onUpdate() {
    //XLOG("DatabaseOpNode::onUpdate");

    if (!m_pDBConnPool) {
        XERR("DatabaseOpNode::onSelect - !m_pDBConnPool");
        return false;
    }

    if (!m_pCol) {
        XERR("DatabaseOpNode::onUpdate !m_pCol tablename:%s", m_strTableName.c_str());
        return false;
    }
    
    DBConnID connID = m_pDBConnPool->getDBConn();
    if ((DBConnID)-1 != connID) {
        uint64 ret = m_pDBConnPool->exeUpdate(connID, m_strTableName.c_str(), (const dbCol *)m_pCol, (const uint8*)m_stData.mBuffer, m_strWhere.c_str());
        m_pDBConnPool->putDBConn(connID);
        if (UInt64_MAX != ret) {
            //XLOG("[数据库操作],DatabaseOpNode::onUpdate - tableName:%s,where:%s.success", m_strTableName.c_str(), m_strWhere.c_str());
            return true;
        }
    }

    XERR("[数据库操作],DatabaseOpNode::onUpdate - tableName:%s,where:%s.fail", m_strTableName.c_str(), m_strWhere.c_str());
    return false;
}

bool DatabaseOpNode::onDelete() {
    //XLOG("DatabaseOpNode::onDelete");

    bool bDelete = onUpdate();
    if (!bDelete) {
        XERR("DatabaseOpNode::onDelete !m_pCol tablename:%s,where:%s", m_strTableName.c_str(), m_strWhere.c_str());
    }

    return bDelete;
}

void DatabaseOpNode::CleanUp() {
    m_enOpType = ODOT_NULL;
    m_pCol = nullptr;
    m_strTableName.clear();
    m_strWhere.clear();
    m_listSeleteData.clear();
}

bool DBOperator::Init(const char* pHost, int iPort, const char* pDBName, const char* pUser, const char* pPassWd, int iPoolCount) {
    m_pConnPool = new DBConnPool();
    if (!m_pConnPool->init(pHost, iPort, pDBName, pUser, pPassWd, iPoolCount)) {
        return false;
    }
    return true;
}

void DBOperator::PushOperator(DatabaseOpNode* pNode) {
    std::lock_guard<std::mutex> lk(m_mLock_);
    m_listDatabaseOpNode.push_back(pNode);
    m_condCv_.notify_one();
}

void DBOperator::Run() {
    std::unique_lock<std::mutex> lk(m_mLock_);
    if (m_listDatabaseOpNode.empty()) {
        m_condCv_.wait_for(lk, std::chrono::seconds(1));
    }
    for (auto& it : m_listDatabaseOpNode) {
        auto* pNode = it;
        pNode->OnCallLogic();
        COperatorMgr::getMe().PushOperator(pNode);
    }
    m_listDatabaseOpNode.clear();

}

void DBOperator::thread_proc() {
    while (xThread::thread_getState() == xThread::THREAD_RUN) {
        Run();
    }
}

