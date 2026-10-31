/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: xOperatorNode.h
      Purpose:   数据库操作节点与异步执行线程声明

      定义 DBDataNode 变长数据缓冲区与 DatabaseOpNode（操作类型/表名/where/列/数据/回调）
      定义 DBOperator 继承 xThread，持 DBConnPool 与条件变量队列异步执行节点
  *****************************************************************/

#pragma once

#include "xBase/xDefine.h"
#include "xBase/xObjPool.h"
#include "xDatabase/xDBMeta.h"
#include "xDatabase/xDBConnPool.h"
#include "xBase/xThread.h"

struct DBDataNode : public xObjPoolBase
{
    uint16 mLen = 0;
    uint16 mMaxLen = 0;
    uint8 *mBuffer = nullptr;

    DBDataNode()
    {
    }
    DBDataNode(const DBDataNode &In)
    {
        if (In.mLen > 0)
        {
            Init(In.mBuffer, In.mLen);
        }
    }
    DBDataNode &operator=(const DBDataNode &In)
    {
        if (this == &In)
        {
            return *this;
        }
        if (In.mLen > 0)
        {
            Init(In.mBuffer, In.mLen);
        }
        return *this;
    }
    void Init(uint8 *buffer, uint16 len)
    {
        if (!buffer || !len)
        {
            return;
        }

        mLen = len;
        if (mMaxLen < len)
        {
            if (mBuffer)
            {
                delete[] mBuffer;
            }
            mMaxLen = len;
            mBuffer = new uint8[mMaxLen];
        }

        memcpy(mBuffer, buffer, mLen);
    }
    ~DBDataNode()
    {
        if (mBuffer)
        {
            delete[] mBuffer;
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
    DatabaseOpNode()
    {
    }
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
    void SetDatabasePool(DBConnPool *pPool)
    {
        m_pDBConnPool = pPool;
    }

public:
    void CleanUp();
    bool OnCallLogic();

private:
    bool onInsert();
    bool onSelect();
    bool onUpdate();
    bool onDelete();

private:
    DBConnPool *m_pDBConnPool = nullptr;
};

typedef std::list<DatabaseOpNode *> DatabaseOpNodeList;

class DBOperator : public xThread
{
public:
    DBOperator()
    {
    }
    ~DBOperator()
    {
    }

private:
    DBConnPool *m_pConnPool = nullptr;

public:
    bool Init(const char *pHost, int32 iPort, const char *pDBName, const char *pUser,
              const char *pPassWd, int32 iPoolCount);
    void PushOperator(DatabaseOpNode *pNode);
    void Run();

private:
    virtual void thread_proc();

private:
    DatabaseOpNodeList m_listDatabaseOpNode;

private:
    std::mutex m_mLock_;
    std::condition_variable m_condCv_;
};
typedef std::unordered_map<std::string, DBOperator *> DBOperatorMap;
