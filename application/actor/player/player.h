#pragma once

#include "xService/xService.h"
#include "xActor/Actor.h"

#include "Database/PlayerTableDefine.h"


#include "../ActorGlobalDefine.h"



class CPlayer : public CActor
{
public:
    CPlayer(ActorID i64ActorID):CActor(ActorType_Player, i64ActorID, "ActorPlayer") { }
    ~CPlayer(){}

private:


public:


public:
    virtual bool Init() {
        
        return true;
    }

    virtual int32 OnRegist() {


        return 0;
    }

    virtual int32 OnUnRegist() {



        return 0;
    }

    virtual CActor* Create(ActorID i64ActorID) {
        return new CPlayer(i64ActorID);
    }

public:
    void Serialize(DatabaseTableCol * pColData) {



    }

    void UnSerialize(const DatabaseTableCol *pColData) {

        MarketDBOrderQueryData *pData = (MarketDBOrderQueryData*)pColData;

        pData->orderid;


    }
};


