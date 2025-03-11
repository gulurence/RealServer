#pragma once

#include "xRecordDefine.h"
#include "xBase/xSingleton.h"
#include "xBase/xObjPool.h"


// 进行数据加载存储更新
/*
 * 1.业务对象 service actor
 * 2.数据管理 mysql redis nats
 * 3.service init
 * 4.actor regist unregist
 */
class CRecordMgr : public xSingleton<CRecordMgr>
{
public:
    CRecordMgr(){
    }
    virtual ~CRecordMgr() {
    }

public:
    bool LoadActor(RecordDataST* pData);
    bool UpdateActor(const RecordDataST* pData);
    bool RemoveActor(const RecordDataST* pData);

public:
    void PushBackRecordData(RecordDataST* pData) {
        m_xPool.DeleteObj(pData);
    }

private:
    xObjPool<RecordDataST> m_xPool;
}; 
