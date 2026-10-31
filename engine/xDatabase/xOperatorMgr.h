/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: xOperatorMgr.h
      Purpose:   数据库操作管理器（单例）

      定义 COperatorMgr 单例，管理 DatabaseOpNode 对象池与 DBOperatorMap（按 title 索引）
      提供 ConnectToDB 建立连接池及 Pop/PushOperatorNode 节点复用接口
  *****************************************************************/

#pragma once

#include "xDBConnPool.h"
#include "xBase/xSingleton.h"
#include "xOperatorNode.h"
#include "xDatabaseDefine.h"
#include "tService/tThridServiceConfig.h"

class COperatorMgr : public xSingleton<COperatorMgr>
{
public:
    COperatorMgr()
    {
    }
    ~COperatorMgr()
    {
    }

public:
    DatabaseOpNode *PopOperatorNode();
    void PushOperatorNode(DatabaseOpNode *pNode);

public:
    bool ConnectToDB(const ThridMysqlConfig &stConfig);
    bool PushOperator(DatabaseOpNode *pNode);

public:
private:
    xObjPool<DatabaseOpNode> m_pDatabaseOpNodePool;

private:
    DBOperatorMap m_mapDBOperator;
};
