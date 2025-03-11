#pragma once

#include "xRecordDefine.h"


class CRedisRecord
{
public:
    CRedisRecord(){}
    virtual ~CRedisRecord() {
    }

private:
    void MakeActorKey(const RecordDataST* pData, std::string &strRedisKey);

public:
    bool Load(RecordDataST *pData);
    bool Update(const RecordDataST* pData);
    bool Remove(const RecordDataST* pData);
};
