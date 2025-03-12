#pragma once

#include "xService/xService.h"

#include "xActor/Actor.h"

#include "../ActorGlobalDefine.h"

#include "xNet/xNetAgent.h"


#include "xDatabase/xDatabaseDefine.h"

#include "GateClient.h"


class CGate : public CActor
{
public:
    CGate(ActorID i64ActorID) :CActor(ActorType_Gate, i64ActorID, "ActorGate") {}
    ~CGate() {}

private:


public:


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
        return new CGate(i64ActorID);
    }

public:
    NetCID NewCID() { return m_u64CID++; }

private:
    xNetAgent* m_pNetAgent = nullptr;

private:
    NetCID m_u64CID = 0;
    GateClientMap m_mapGateClient;
};















