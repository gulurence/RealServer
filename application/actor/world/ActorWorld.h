#pragma once

#include "xService/xService.h"
#include "xActor/Actor.h"

#include "Database/WorldTableDefine.h"


#include "../ActorGlobalDefine.h"



class CActorWorld : public CActor
{
public:
    CActorWorld(ActorID i64ActorID):CActor(ActorType_World, i64ActorID, "ActorWorld") { }
    ~CActorWorld(){}

private:


public:

public:
    virtual bool Install() {

        return true;
    }

    virtual bool UnInstall() {

        return true;
    }

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
        return new CActorWorld(i64ActorID);
    }

public:
    void Serialize(DatabaseTableCol * pColData) {



    }

    void UnSerialize(const DatabaseTableCol *pColData) {

        WorldObjectQueryData*pData = (WorldObjectQueryData*)pColData;

        pData->orderid;


    }
};


