/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: xDatabaseRecord.h
      Purpose:   MySQL 数据库记录声明

      CDatabaseRecord 封装 Load/Update/Remove 三步异步 DB 操作, 连接池名默认 actor_db
  *****************************************************************/

#pragma once

#include "xRecordDefine.h"

#include "xNet/xNetDefine.h"
#include <google/protobuf/message.h>

class CDatabaseRecord
{
public:
    // strPoolTitle must match a pool registered with COperatorMgr::ConnectToDB().
    // Default "actor_db" matches the default server.xml entry.
    CDatabaseRecord(const std::string &strPoolTitle = "actor_db") : m_strPoolTitle(strPoolTitle)
    {
    }
    virtual ~CDatabaseRecord()
    {
    }

public:
    bool Load(RecordDataST *pData);
    bool Update(const RecordDataST *pData);
    bool Remove(const RecordDataST *pData);

private:
    std::string m_strPoolTitle;
};
