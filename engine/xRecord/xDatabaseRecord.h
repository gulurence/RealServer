#pragma once

#include "xRecordDefine.h"

#include "xNet/xNetDefine.h"
#include <google/protobuf/message.h>



class CDatabaseRecord
{
public:
    CDatabaseRecord(){}
    virtual ~CDatabaseRecord() {
    }

public:
    bool Load(RecordDataST* pData);
    bool Update(const RecordDataST* pData);
    bool Remove(const RecordDataST* pData);
};

