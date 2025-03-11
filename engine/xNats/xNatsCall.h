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
    void Resize(const int32& i32Len) {
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
#define NatsRequestCall(strSubject, reqMsg, respMsg) {\
    auto* pNatsCon = NatsConnPool::getMe().GetNatsConnect();\
    STProtoSerializeData stReqData(reqMsg->ByteSize());\
    reqMsg->SerializeToArray(stReqData.GetBuffer(), stReqData.Capacity());\
    STProtoSerializeData respData(65535);\
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



class SNatsService : public xSingleton<SNatsService>, xThread
{
public:
    SNatsService(){}
    ~SNatsService() {}

public:
    void Start(uint16 u16Port);

    virtual void thread_proc();

    void RegistService(grpc::Service* pService);

private:
    uint16 m_u16Port = 0;
    std::atomic<bool> m_bRunning = true;
    grpc::ServerBuilder builder;
    std::shared_ptr<grpc::Server> m_pServicePtr;
};


class CNatsService : public xSingleton<CNatsService>
{
public:
    CNatsService() {}
    ~CNatsService() {}

public:
    std::shared_ptr<grpc::Channel> GetChannel(const std::string& strServerName);

public:
    /********
    * 链接到 GRPC 服务器
    * 
    * strServerName - 服务器昵称
    * strIp - 目标服务器IP
    * u16Port - 目标服务器端口
    */
    bool ConnectToServer(const std::string& strServerName, int32 i32PoolCount, const std::string& strIp, uint16 u16Port);

    /********
    * 链接到本地 GRPC 服务器
    *
    * strServerName - 服务器昵称
    * u16Port - 目标服务器端口
    */
    bool ConnectToLocalServer(const std::string& strServerName, int32 i32PoolCount, uint16 u16Port);

private:
    std::map<std::string, std::pair<int32, int32> > m_mapChannelSize;
    std::map<std::string, std::vector<std::shared_ptr<grpc::Channel>> > m_mapChannel;
};


//#include <grpcpp/grpcpp.h>
//#include "rpc/myservice.grpc.pb.h"
//
//// 实现 MyService 服务
//class MyServiceImpl final : public myservice::MyService::Service
//{
//public:
//    // 实现 SayHello RPC 方法
//    grpc::Status SayHello(grpc::ServerContext* context, const myservice::HelloRequest* request, myservice::HelloReply* reply) override {
//        // 构造一个回复消息
//        std::string prefix("Hello, ");
//        reply->set_message(prefix + request->name());
//
//        // 返回成功状态
//        return grpc::Status::OK;
//    }
//};
