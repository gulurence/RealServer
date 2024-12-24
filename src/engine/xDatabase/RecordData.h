#pragma once

#include "../xBase/xThread.h"
#include "../xBase/xObjPool.h"
#include "RecordDef.h"
#include "RecordSaveTimer.h"
#include "xDBConnPool.h"

// 鏁版嵁鐨勬洿鏂版柟寮?
enum RECORD_OP_TYPE
{
    ROT_DEFUALT = 0,
    ROT_INIT,
    ROT_INSERT,
    ROT_UPDATE,
    ROT_DELETE,
};
struct RecordCacheData : public xObjPoolBase
{
    RECORD_OP_TYPE databaseOpType = ROT_DEFUALT;
    int size = 0;
    int maxsize = 0;
    char *buffer = nullptr;
    RecordCacheData() {
        maxsize = 1;
        buffer = new char[maxsize];
    }
    RecordCacheData(const char *_buffer, const uint32 _len) {
        setData(_buffer, _len);
    }
    ~RecordCacheData() {
        if (buffer) {
            delete []buffer;
            buffer = nullptr;
        }
    }
    RecordCacheData &operator=(const RecordCacheData& In) {
        if (this == &In)return *this;
        databaseOpType = In.databaseOpType;
        setData(In.buffer, In.size);
        return *this;
    }
    void set_size(const int _size) {
        size = _size;
    }
    void resize(const int len) {
        if (len > maxsize) {
            if (buffer) {
                delete[]buffer;
            }

            size = len;
            maxsize = len;
            buffer = new char[maxsize];
        }
    }
    char *getBuffer() {
        return buffer;
    }
    void setData(const char *_buffer, const uint32 _len) {
        if (_len <= 0) {
            return;
        }

        resize(_len);
        memcpy(buffer, _buffer, _len);
    }
};

class RecordData : public xThread, public RecordSaveTimer
{
    typedef std::map<uint64, RecordCacheData> RecordDataMap;

public:
    RecordData(){
    }
    virtual ~RecordData() {}

protected:
    // 鏁版嵁搴撹〃鍚嶇О
    std::string            m_strTableName;
    // 鍔犺浇鐨勬椂鍊欏叕鐢ㄧ殑缂撳瓨 鍖?
    uint32                m_cacheSize = 0;
    char                *m_pCacheBuffer = nullptr;

public:
    char *GetEmptyCache() {
        if (m_cacheSize > 0) {
            memset(m_pCacheBuffer, 0, m_cacheSize);
        }
        return m_pCacheBuffer;
    }

protected:
    // 鎿嶄綔鏁版嵁搴撳叕鍏遍攣
    std::mutex          m_lockDBOP;
    // 鏁版嵁搴撻摼鎺ユ睜ID
    DBConnID            m_dbPoolID = 0;
    dbCol               *m_pCol = nullptr;

    DBConnPool          *m_dbConnPool = nullptr;

protected:
    // 淇濆瓨鎵€鏈夌紦瀛樼殑 鍏叡閿?
    std::mutex           m_lockData;
    RecordDataMap        m_mapData;
    
private:
    virtual void thread_proc() final;
    virtual void OnSaveTimer(uint64 uID) final;

public:
    // pDBCol - 瀛楁瀹氫箟缁撴瀯
    // cacheSize - select 浣跨敤鐨勬渶澶х紦瀛樺ぇ灏?
    // tableName - 瀵瑰簲鐨勬暟鎹簱琛ㄥ悕绉?
    // strTabelKey - 鏁版嵁搴搆ey鍚嶇О
    bool Init(DBConnPool *dbConnPool, dbCol *pDBCol, const std::string &tableName, int iCacheSize = RECODE_LOAD_MAX_CACHE_SIZE);

public:
    template<class T>
    bool GetTRecordFromCache(uint64 uID, T &outData) {
        RecordCacheData *stData = nullptr;
        if (GetRecordFromCache(uID, stData)) {
            memcpy(&outData, (stData)->buffer, sizeof(T));
            return true;
        }
        return false;
    }

public:
    bool HasRecordData(uint64 uID);
    bool GetRecordFromCache(uint64 uID, RecordCacheData *&outData);
    bool InsertToDB(uint64 uID, const RecordCacheData &outData);
    void InitRecordToCache(uint64 uID, const RecordCacheData &stData);
    void SetRecordToCache(uint64 uID, const RecordCacheData &stData, bool synchroToDB=false, uint32 delaySaveTime = RECORD_DELAY_SAVE_TIME);
    void DelRecordCache(uint64 uID);
};





