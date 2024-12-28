#pragma once

#include "xBase/xDefine.h"
#include "xBase/xSingleton.h"
#include <grpcpp/grpcpp.h>
#include "xBase/xThread.h"


#define GRPCRequest(ServerName,Namespace,Service,TRequest,TReply,TCallFun,request,reply,status){\
    auto channel___ = CRpcService::getMe().GetChannel(ServerName);\
    std::unique_ptr<Namespace::Service::Stub> stub___ = Namespace::Service::NewStub(channel___);\
    grpc::ClientContext context___;\
    status = stub___->TCallFun(&context___, request, reply);\
    if (status.ok()) {\
        std::cout << "Server response: " << reply.message() << std::endl;\
    } else {\
        std::cout << "gRPC failed: " << status.error_message() << std::endl;\
    }\
}


struct CorutineContinue
{
    std::chrono::milliseconds duration;
    bool await_ready() {
        return false;  // 立即完成
    }

    void await_suspend(std::coroutine_handle<> h) {
        std::cout << "RPC request started, will take " << duration.count() << " milliseconds." << std::endl;
        std::thread([h, this] {
            std::this_thread::sleep_for(std::chrono::seconds(100)); // 模拟RPC延迟
            h.resume();  // RPC完成，恢复协程
            }).detach();

        //std::this_thread::sleep_for(std::chrono::seconds(2)); // 模拟RPC延迟
        //h.resume();  // RPC完成，恢复协程
    }

    void await_resume() {
        std::cout << "RPC request completed." << std::endl;
    }

    void Return_Value() {}
};


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
