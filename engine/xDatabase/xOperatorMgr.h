#pragma once

#include "xDBConnPool.h"
#include "xBase/xSingleton.h"
#include "xOperatorNode.h"
#include "xDatabaseDefine.h"


class COperatorMgr : public xSingleton<COperatorMgr>
{
public:
    COperatorMgr() {}
    ~COperatorMgr() {}

public:
    DatabaseOpNode* PopOperatorNode();
    void PushOperatorNode(DatabaseOpNode* pNode);

public:
    bool ConnectToDB(const DBConfigST& stConfig);
    bool PushOperator(DatabaseOpNode* pNode);

public:


private:
    xObjPool<DatabaseOpNode> m_pDatabaseOpNodePool;

private:
    DBOperatorMap m_mapDBOperator;
};

