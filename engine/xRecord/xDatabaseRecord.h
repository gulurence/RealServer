#pragma once

#include "xRecordDefine.h"

#include "xNet/xNetDefine.h"
#include <google/protobuf/message.h>



class CDatabaseRecord
{
public:
    // strPoolTitle must match a pool registered with COperatorMgr::ConnectToDB().
    // Default "actor_db" matches the default server.xml entry.
    CDatabaseRecord(const std::string& strPoolTitle = "actor_db")
        : m_strPoolTitle(strPoolTitle) {}
    virtual ~CDatabaseRecord() {
    }

public:
    bool Load(RecordDataST* pData);
    bool Update(const RecordDataST* pData);
    bool Remove(const RecordDataST* pData);

private:
    std::string m_strPoolTitle;
};

