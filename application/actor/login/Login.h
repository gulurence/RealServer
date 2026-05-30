#pragma once

#include "xService/xService.h"
#include "xActor/Actor.h"
#include "xNet/xNetAgent.h"
#include "xDatabase/xDatabaseDefine.h"
#include "Database/LoginTableDefine.h"

#include "../ActorGlobalDefine.h"



class CLogin : public CActor
{
public:
    CLogin(ActorID i64ActorID) :CActor(ActorType_Login, i64ActorID, "ActorLogin") {}
    ~CLogin() {}

private:

public:
    virtual bool Install() {

        return false;
    }

    virtual bool UnInstall() {

        return false;
    }

public:
    virtual bool Init();

public:
    virtual int32 OnRegist() {


        return 0;
    }

    virtual int32 OnUnRegist() {



        return 0;
    }

    virtual CActor* Create(ActorID i64ActorID) {
        return new CLogin(i64ActorID);
    }

public:
    void Serialize(DatabaseTableCol* pColData) {

    }

    void UnSerialize(const DatabaseTableCol* pColData) {

        MarketDBOrderQueryData* pData = (MarketDBOrderQueryData*)pColData;

        pData->orderid;

    }

private:
    xNetAgent* m_pNetAgent = nullptr;
};















