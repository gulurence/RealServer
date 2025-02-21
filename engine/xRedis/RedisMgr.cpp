#include "RedisMgr.h"
#include "xBase/xXMLParser.h"
//#include "xSConnect/SStartConfig.h"


CRedisMgr::CRedisMgr() {
}

CRedisMgr::~CRedisMgr() {
}

bool CRedisMgr::Init(const char* hostName, const int &port, const char *auth, const int& dbIndex, int32 i32PoolCount) {
    m_i32PoolCount = i32PoolCount;
    if (m_i32PoolCount < 2) {
        m_i32PoolCount = 2;
    } else if (m_i32PoolCount > 8) {
        m_i32PoolCount = 8;
    }

    for (int i = 0;i<m_i32PoolCount;++i) {
        auto* pCli = new CRedisCli();
        if (pCli->ConnectDB(hostName, port) != REDIS_OK) {
            Release();
            return false;
        }
        if (pCli->Auth(auth) != REDIS_OK) {
            Release();
            return false;
        }
        if (pCli->SelectDB(dbIndex) != REDIS_OK) {
            Release();
            return false;
        }
        m_listRedisCliFree.push_back(pCli);
    }
    
    return true;
}

void CRedisMgr::Release() {
    for (auto &it: m_listRedisCli) {
        delete it;
    }
    for (auto& it : m_listRedisCliFree) {
        delete it;
    }
    m_listRedisCli.clear();
    m_listRedisCliFree.clear();
}

CRedisCli* CRedisMgr::PopRedisCli() {
    if (m_listRedisCliFree.empty()) {
        return nullptr;
    }
    auto *pCli = m_listRedisCliFree.front();
    m_listRedisCliFree.pop_front();
    return pCli;
}


void CRedisMgr::PushRedisCli(CRedisCli*pCli) {
    m_listRedisCliFree.push_back(pCli);
}

