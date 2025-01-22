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

#define GRPCRequestCall(ServerName,Namespace,Service,TRequest,TReply,TCallFun,pService__,request__, reply__, status__)\
struct CorutineContinue\
{\
    xService* pService_;\
    Namespace::TRequest &request_;\
    Namespace::TReply &reply_;\
    grpc::Status &status_;\
    bool await_ready() {\
        return false; \
    }\
    void await_suspend(std::coroutine_handle<> h) {\
        std::cout << "RPC request started, will take milliseconds." << std::endl;\
        std::thread([h, this] {\
            xService* pServiceTmp = pService_;\
            GRPCRequest(ServerName, Namespace, Service, TRequest, TReply, TCallFun, request_, reply_, status_)\
            h.resume(); \
            auto pScheduler = pServiceTmp->GetServiceScheduler();\
            pScheduler->running_ = nullptr;\
            pScheduler->requests_.Pop(pScheduler->running_);\
            pScheduler->SetSchedulerState(ServiceSchedulerRunning);\
            }).detach();\
    }\
    void await_resume() {\
        std::cout << "RPC request completed." << std::endl;\
    }\
    void Return_Value() {}\
};co_await CorutineContinue(pService__,request__,reply__,status__);

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
    bool ConnectToServer(const std::string& strServerName, uint16 u16Port);

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
