#pragma once

#include "xBase/xDefine.h"
#include "xBase/xSingleton.h"
#include <grpcpp/grpcpp.h>
#include "xBase/xThread.h"



#define GRPCRequest(ServerName,Namespace,Service,TRequest,TReply,TCallFun,request,reply,status){\
    auto channel___ = CRpcService::getMe().GetChannel(ServerName);\
    std::unique_ptr<Namespace::Service::Stub> stub___ = Namespace::Service::NewStub(channel___);\
    grpc::ClientContext context___;\
    status = stub___->TCallFun(&context___, request, &reply);\
    if (status.ok()) {\
        std::cout << "Server response: " << reply.message() << std::endl;\
    } else {\
        std::cout << "gRPC failed: " << status.error_message() << std::endl;\
    }\
}


struct EmptyCorutine
{
    bool await_ready() {return true;}
    void await_suspend(std::coroutine_handle<> h) {}
    void await_resume() {}
    void return_value() {}
};

// 协程异步调用GRPC
// ServerName - GRPC服务器名称
#define GRPCRequestCall(ServerName,Namespace,Service,TRequest,TReply,TCallFun,ptrEvent__,pService__,request__,reply__,status__)\
struct CorutineContinue\
{\
    EventScheduler* ptrEvent_;\
    Namespace::TRequest &request_;\
    Namespace::TReply &reply_;\
    grpc::Status &status_;\
    bool await_ready() {\
        return false; \
    }\
    void await_suspend(std::coroutine_handle<> h) {\
        std::cout << "RPC request started, will take milliseconds." << std::endl;\
        std::thread([this,h] {\
            EventScheduler* ptrEvent = ptrEvent_;\
            GRPCRequest(ServerName, Namespace, Service, TRequest, TReply, TCallFun, request_, reply_, status_)\
            auto pScheduler = ptrEvent->GetServiceScheduler();\
            pScheduler->SetSchedulerState(SchedulerStateType_Blocked_End);\
            h.resume();\
            }).detach();\
    }\
    void await_resume() {\
        std::cout << "RPC request completed." << std::endl;\
    }\
    void Return_Value() {}\
};co_await CorutineContinue(ptrEvent__,request__,reply__,status__);


class SRpcService : public xSingleton<SRpcService>, xThread
{
public:
    SRpcService(){}
    ~SRpcService() {}

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


class CRpcService : public xSingleton<CRpcService>
{
public:
    CRpcService() {}
    ~CRpcService() {}

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
    bool ConnectToServer(const std::string& strServerName, const std::string& strIp, uint16 u16Port);

    /********
    * 链接到本地 GRPC 服务器
    *
    * strServerName - 服务器昵称
    * u16Port - 目标服务器端口
    */
    bool ConnectToLocalServer(const std::string& strServerName, uint16 u16Port);

private:
    std::map<std::string, std::shared_ptr<grpc::Channel>> m_mapChannel;
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
