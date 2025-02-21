#pragma once

#include "xEventDefine.h"

#include "xNet/xNetDefine.h"

#include <google/protobuf/message.h>

class xEvent
{
public:
    xEvent();
    virtual ~xEvent();

private:
    EventType m_enType;

private:
    uint64 m_u64SrcID;
    uint64 m_u64TargetID;
};

typedef std::shared_ptr<google::protobuf::Message> PbMsgPtr;

// 网络协议 event
class xPBEvent : public xEvent
{
public:
    xPBEvent(google::protobuf::Message* req, google::protobuf::Message* rsp):m_pRequest(req), m_pResponse(rsp){}
    virtual ~xPBEvent() {
        m_pRequest = nullptr;
        m_pResponse = nullptr;
    }

public:
    google::protobuf::Message* Request() {
        return m_pRequest;
    }
    google::protobuf::Message* Response() {
        return m_pResponse;
    }

private:
    google::protobuf::Message *m_pRequest;
    google::protobuf::Message *m_pResponse;
};

typedef std::shared_ptr<xPBEvent> PBEventPtr;


// 网络协议 event
class xActorEvent : public xEvent
{
public:
    xActorEvent(NetPackageSharedPtr ptr) :m_pPackage(ptr) {}
    virtual ~xActorEvent() { m_pPackage = nullptr; }

public:
    NetPackageSharedPtr Get() {
        return m_pPackage;
    }

private:
    NetPackageSharedPtr m_pPackage;
};

