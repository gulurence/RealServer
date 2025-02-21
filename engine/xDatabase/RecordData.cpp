#include "RecordData.h"
#include "RecordFun.h"

void RecordData::thread_proc() {
    while (thread_getState() == xThread::THREAD_RUN) {
#ifdef _WIN32
        Sleep(5);
#else
        Sleep(10);
#endif

         RecordSaveTimer::RunTimer(time(NULL));
    }
}

void RecordData::OnSaveTimer(uint64 uID) {
    if (!m_dbConnPool) {
        XERR("RecordData::OnSaveTimer !m_dbConnPool");
        return;
    }
    RecordCacheData *stData = nullptr;
    if (RecordData::GetRecordFromCache(uID, stData)) {
        char strWhere[256] = { 0 };
        snprintf(strWhere, sizeof(strWhere), "UID=%llu", uID);
        m_lockDBOP.lock();
        if (ROT_UPDATE == stData->databaseOpType) {
            RecordFun::Update(m_dbConnPool, m_dbPoolID, m_pCol, (const unsigned char*)&(stData)->buffer[0], m_strTableName, strWhere);
        }
        else if (ROT_INSERT == stData->databaseOpType) {
            RecordFun::Insert(m_dbConnPool, m_dbPoolID, m_strTableName,  m_pCol, (const unsigned char*)&(stData)->buffer[0]);
        } else {
            XERR("RecordData::OnSaveTimer not set databaseOpType !!! uID:%llu ", uID);
        }
        m_lockDBOP.unlock();
    }
}

bool RecordData::Init(DBConnPool *dbConnPool, dbCol *pDBCol, const std::string &tableName, int iCacheSize) {
    if (!dbConnPool || !pDBCol) {
        XERR("RecordData::Init !dbConnPool || !pDBCol");
        return false;
    }
    m_dbConnPool = dbConnPool;
    m_dbPoolID = dbConnPool->getDBConn();
    if ((DBConnID)-1 == m_dbPoolID) {
        return false;
    }

    m_pCol = pDBCol;
    m_cacheSize = iCacheSize;
    m_strTableName = tableName;
    m_pCacheBuffer = new char[iCacheSize];
    return true;
}

bool RecordData::HasRecordData(uint64 uID) {

    m_lockData.lock();
    auto it = m_mapData.find(uID);
    if (it != m_mapData.end()) {
        m_lockData.unlock();
        return true;
    }
    m_lockData.unlock();

    return false;
}

bool RecordData::GetRecordFromCache(uint64 uID, RecordCacheData *&outData) {
    bool bRet = false;
    m_lockData.lock();
    auto it = m_mapData.find(uID);
    if (it != m_mapData.end()) {
        bRet = true;
        outData = &it->second;
    }
    m_lockData.unlock();

    return bRet;
}

bool RecordData::InsertToDB(uint64 uID, const RecordCacheData &outData) {
    if (!m_dbConnPool) {
        XERR("RecordData::InsertToDB !m_dbConnPool");
        return false;
    }
    bool bRet = false;
    m_lockDBOP.lock();
    bRet = RecordFun::Insert(m_dbConnPool, m_dbPoolID, m_strTableName, m_pCol, (const unsigned char*)&outData.buffer[0]);
    m_lockDBOP.unlock();
    return bRet;
}

bool isMemCmp(const unsigned char *src, const unsigned char *target, int size) {
    for (int i = 0; i < size; ++i) {
        if ((int)src[i] != (int)target[i]) {
            return false;
        }
    }

    return true;
}

void RecordData::InitRecordToCache(uint64 uID, const RecordCacheData &stData) {
    if (!m_dbConnPool) {
        XERR("RecordData::SetRecordToCache !m_dbConnPool");
        return;
    }

    m_lockData.lock();
    if (m_mapData.find(uID) != m_mapData.end()) {
        m_lockData.unlock();
        return;
    }

    m_mapData[uID] = stData;

    m_lockData.unlock();    
}

void RecordData::SetRecordToCache(uint64 uID, const RecordCacheData &stData, bool synchroToDB, uint32 delaySaveTime) {
    if (!m_dbConnPool) {
        XERR("RecordData::SetRecordToCache !m_dbConnPool");
        return ;
    }


    m_lockData.lock();
    auto it = m_mapData.find(uID);
    if (it != m_mapData.end()) {
        // 直接对比下内存数据
        if (stData.size == it->second.size && isMemCmp((unsigned char *)it->second.buffer, (unsigned char *)stData.buffer, stData.size)) {
            m_lockData.unlock();
            return;
        }
    } 
    m_mapData[uID] = stData;
    m_lockData.unlock();

    if (ROT_DEFUALT == stData.databaseOpType) {
        // 只是加载而已
        return;
    }

    if (synchroToDB) {
        OnSaveTimer(uID);
    } else {
        AddTimer(uID, delaySaveTime);
    }
}

void RecordData::DelRecordCache(uint64 uID) {
    m_lockData.lock(); 
    m_mapData.erase(uID);
    m_lockData.unlock();
}



