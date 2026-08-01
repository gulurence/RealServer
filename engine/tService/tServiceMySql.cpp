#include "tServiceMySql.h"
#include "xDatabase/xOperatorMgr.h"
#include "xLog/xLog.h"

bool tServiceMySql::Init(const ThridServiceConfig& cfg) {
    if (m_bInitialized) {
        XWRN("[tServiceMySql] Already initialized: %s", m_strName.c_str());
        return true;
    }

    auto dbCfg = cfg.mysql;
    dbCfg.title = m_strName;

    if (!COperatorMgr::getMe().ConnectToDB(dbCfg)) {
        XERR("[tServiceMySql] Init failed: %s @ %s:%d/%s",
             m_strName.c_str(), dbCfg.host.c_str(), dbCfg.port, dbCfg.dbName.c_str());
        return false;
    }

    m_bInitialized = true;
    XLOG("[tServiceMySql] Init ok: %s @ %s:%d/%s (pool=%d)",
         m_strName.c_str(), dbCfg.host.c_str(), dbCfg.port, dbCfg.dbName.c_str(), dbCfg.poolCount);
    return true;
}

void tServiceMySql::Release() {
    if (!m_bInitialized) return;

    // COperatorMgr 目前没有 Disconnect 接口；
    // 实际关闭由 DBOperator 析构处理。
    XLOG("[tServiceMySql] Released: %s", m_strName.c_str());
    m_bInitialized = false;
}
