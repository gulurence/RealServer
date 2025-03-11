#pragma once

#include "xBase/xDefine.h"
#include "xBase/xObjPool.h"
#include "xDatabase/xDBMeta.h"
#include "xDatabase/xDBConnPool.h"
#include "xBase/xThread.h"


struct DBDataNode : public xObjPoolBase
{
    unsigned short mLen = 0;
    unsigned short mMaxLen = 0;
    unsigned char *mBuffer = nullptr;

    DBDataNode() {}
    DBDataNode(const DBDataNode& In) {
        if (In.mLen > 0) {
            Init(In.mBuffer, In.mLen);
        }
    }
    DBDataNode &operator=(const DBDataNode& In) {
        if (this == &In)return *this;
        if (In.mLen > 0) {
            Init(In.mBuffer, In.mLen);
        }
        return *this;
    }
    void Init(unsigned char *buffer, unsigned short len) {
        if (!buffer || !len) {
            return;
        }

        mLen = len;
        if (mMaxLen < len) {
            if (mBuffer)
                delete[]mBuffer;
            mMaxLen = len;
            mBuffer = new unsigned char[mMaxLen];
        }

        memcpy(mBuffer, buffer, mLen);
    }
    ~DBDataNode() {
        if (mBuffer) {
            delete[]mBuffer;
            mBuffer = nullptr;
        }
    }
};

typedef std::vector<DBDataNode> DBDATA_NODE_VECTOR;

enum ORDER_DB_OP_TYPE
{
    ODOT_NULL = 0,
    ODOT_INSERT,
    ODOT_SELECT,
    ODOT_UPDATE,
    ODOT_DELETE,
};

typedef std::function<void(bool)> DBOpCallBack;
class DatabaseOpNode : public xObjPoolBase
{
public:
    DatabaseOpNode(){}
    ~DatabaseOpNode();

public:
    std::string m_stTitle;

public:
    ORDER_DB_OP_TYPE m_enOpType = ODOT_NULL;
    std::string m_strTableName;
    std::string m_strWhere;
    dbCol *m_pCol = nullptr;
    DBDataNode m_stData;

public:
    DBDATA_NODE_VECTOR m_listSeleteData;

public:
    DBOpCallBack m_pDBOpCallBack = nullptr;

public:
    uint64 m_u64SteamID = 0;

public:
    void SetDatabasePool(DBConnPool* pPool) { m_pDBConnPool = pPool; }

public:
    void CleanUp();
    bool OnCallLogic();

private:
    bool onInsert();
    bool onSelect();
    bool onUpdate();
    bool onDelete();

private:
    DBConnPool* m_pDBConnPool = nullptr;
};

typedef std::list<DatabaseOpNode*> DatabaseOpNodeList;



class DBOperator : public xThread
{
public:
    DBOperator() {}
    ~DBOperator() {}

private:
    DBConnPool* m_pConnPool = nullptr;

public:
    bool Init(const char* pHost, int iPort, const char* pDBName, const char* pUser, const char* pPassWd, int iPoolCount);
    void PushOperator(DatabaseOpNode* pNode);
    void Run();

private:
    virtual void thread_proc();

private:
    DatabaseOpNodeList m_listDatabaseOpNode;

private:
    std::mutex m_mLock_;
    std::condition_variable m_condCv_;
};
typedef std::unordered_map<std::string, DBOperator*> DBOperatorMap;


