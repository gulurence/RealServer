#pragma once

#include "xNatsDefine.h"
#include "xBase/xSingleton.h"
#include "xBase/xThread.h"
#include "xBase/xCircularPool.h"
#include "xScheduler/xScheduler.h"

#include "xNats.h"
#include "google/protobuf/message.h"




struct STProtoSerializeData
{
private:
    char* m_pData = nullptr;
    uint32 m_u32Len = 0;
    uint32 m_u32Capacity = 0;

public:
    STProtoSerializeData(const int32& i32Len) {
        if (i32Len <= 0) {
            return;
        }
        if (m_pData) {
            delete[]m_pData; m_pData = nullptr;
        }
        m_u32Len = 0;
        m_u32Capacity = i32Len;
        m_pData = new char[m_u32Capacity];
    }
    ~STProtoSerializeData() {
        if (m_pData) {
            delete[]m_pData; m_pData = nullptr;
        }
        m_u32Len = 0;
        m_u32Capacity = 0;
    }

public:
    void Resize(const uint32& i32Len) {
        if (i32Len > m_u32Len) {
            if (m_pData) {
                delete[]m_pData; m_pData = nullptr;
            }
        }
        m_u32Len = 0;
        m_u32Capacity = i32Len;
        m_pData = new char[m_u32Capacity];
    }
    char* GetBuffer() {
        return m_pData;
    }
    uint32& MutableSize() {
        return m_u32Len;
    }
    uint32 Size() {
        return m_u32Len;
    }
    int32 Capacity() {
        return m_u32Capacity;
    }
};


// #define NatsRequestCall(const std::string& strSubject, google::protobuf::Message* reqMsg, google::protobuf::Message* respMsg)
#define NatsRequestCall(strSubject, reqMsg, respMsg, status_) {\
    auto* pNatsCon = NatsConnPool::getMe().getConnection();\
    STProtoSerializeData stReqData(reqMsg->ByteSize());\
    reqMsg->SerializeToArray(stReqData.GetBuffer(), stReqData.Capacity());\
    STProtoSerializeData respData(NATS_RESPONSE_BUFF_SIZE);\
    status_ = pNatsCon->RequestMsg("", stReqData.GetBuffer(), stReqData.Capacity(), respData.GetBuffer(), respData.MutableSize());\
    if (status_ == NATS_OK) {\
        request_->SerializeToArray(respData.GetBuffer(),respData.Size());\
        std::cout << "Server response: " << status_ << std::endl; \
    } else {\
        std::cout << "gRPC failed: " << status_ << std::endl; \
    }\
}


// 协程异步调用GRPC
// ServerName - GRPC服务器名称
#define NatsRequest(strSubject, request__, response__, ptrEvent__, status__) {\
    struct CorutineContinue\
    {\
        EventScheduler* ptrEvent_ = nullptr;\
        google::protobuf::Message* request_ = nullptr;\
        google::protobuf::Message* response_ = nullptr;\
        int32& status_;\
        bool await_ready() {\
            return false; \
        }\
        void await_suspend(std::coroutine_handle<> h) {\
            std::cout << "RPC request started, will take milliseconds." << std::endl;\
            RpcCallMgr::getMe().AddCall([this,h] {\
                EventScheduler* ptrEvent = ptrEvent_;\
                NatsRequestCall(ServerName, request_, response_, status_)\
                auto pScheduler = ptrEvent->GetServiceScheduler();\
                pScheduler->SetSchedulerState(SchedulerStateType_Blocked_End);\
                h.resume();\
                });\
        }\
        void await_resume() {\
            std::cout << "RPC request completed." << std::endl;\
        }\
        void Return_Value() {}\
    };co_await CorutineContinue(ptrEvent__,&request__,&response__,status__);\
}



typedef std::function<void()> OnEventRpcCallBack;
class NatsCallMgr : public xSingleton<NatsCallMgr>
{
public:
    NatsCallMgr(){}
    ~NatsCallMgr(){}

public:
    SchedulerTask RpcCall(OnEventRpcCallBack pCall) {
        pCall();
        co_return;
    }

    void AddCall(OnEventRpcCallBack pCall);
    void Init();
    OnEventRpcCallBack PopCall();

public:
    std::atomic<bool> m_isRunning = true;

private:
    xCircularPool<OnEventRpcCallBack> m_poolEvent;
};





