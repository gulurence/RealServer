//#include "xBase/xDefine.h"

//#include "xScheduler/xScheduler.h"

#include "xRpc/xRpc.h"

#include "xEvent/xEvent.h"
#include "xService/xService.h"
#include "xEvent/xEventDispatcher.h"


//#include "rpc/myservice.grpc.pb.h"

#include "msg/login.pb.h"


/*

// 
    创建 gRPC 客户端存根
    std::unique_ptr<myservice::MyService::Stub> stub = myservice::MyService::NewStub(channel);
    // 执行 RPC 请求
    grpc::ClientContext context;
    grpc::Status status = stub->SayHello(&context, request, &reply);
    // 处理响应
    if (status.ok()) {
        std::cout << "Server response: " << reply.message() << std::endl;
    } else {
        std::cout << "gRPC failed: " << status.error_message() << std::endl;
    }
*/

// 协议处理函数 1
SchedulerTask TestEventCall_1(xService* pService, PBEventPtr msgPtr) {
    auto *pRequest = (PbMsg::LoginRequest*)(msgPtr->Request());
    auto *pResponse = (PbMsg::LoginResponse*)(msgPtr->Response());

    std::cout << "TestEventCall_1111 begin " << pService->Id() <<" index:"<< pRequest->index() << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    //// 构造请求
    //myservice::HelloRequest request;
    //request.set_name("World");
    //// 构造响应
    //myservice::HelloReply reply;

    //grpc::Status status;
    ////GRPCRequest("", myservice, MyService, HelloRequest, HelloReply, SayHello, request, &reply, status);
    //auto channel___ = CRpcService::getMe().GetChannel("");
    //std::unique_ptr<myservice::MyService::Stub> stub = myservice::MyService::NewStub(channel___);
    //// 执行 RPC 请求
    //grpc::ClientContext context;
    //status = stub->SayHello(&context, request, &reply);
    //// 处理响应
    //if (status.ok()) {
    //    std::cout << "Server response: " << reply.message() << std::endl;
    //} else {
    //    std::cout << "gRPC failed: " << status.error_message() << std::endl;
    //}

    //// 处理响应
    ////if (status.ok()) {
    ////    std::cout << "Server response: " << reply.message() << std::endl;
    ////} else {
    ////    std::cout << "gRPC failed: " << status.error_message() << std::endl;
    ////}


    co_await CorutineContinue{ std::chrono::milliseconds(10) };

    std::cout << "TestEventCall_1111 end " << pService->Id() << std::endl;
}

SchedulerTask TestEventCall_2(xService* pService, PBEventPtr msgPtr) {
    auto* pRequest = (PbMsg::LoginRequest*)(msgPtr->Request());
    auto* pResponse = (PbMsg::LoginResponse*)(msgPtr->Response());


    std::cout << "TestEventCall_2222 begin " << pService->Id() << " index:" << pRequest->index() << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    //// 构造请求
    //myservice::HelloRequest request;
    //request.set_name("World");
    //// 构造响应
    //myservice::HelloReply reply;

    //grpc::Status status;
    //GRPCRequest("", myservice, MyService, HelloRequest, HelloReply, SayHello, request, &reply, status);
    //// 处理响应
    //if (status.ok()) {
    //    std::cout << "Server response: " << reply.message() << std::endl;
    //} else {
    //    std::cout << "gRPC failed: " << status.error_message() << std::endl;
    //}


    co_await CorutineContinue{ std::chrono::milliseconds(50) };


    std::cout << "TestEventCall_2222 end " << pService->Id() << " index" << pRequest->index() << std::endl;
}

//
struct RegistDispatcher___{
    RegistDispatcher___() {

        xEventDispatcher::getMe().RegistProtoCallBack(100, 100, TestEventCall_1);
        xEventDispatcher::getMe().RegistProtoCallBack(100, 101, TestEventCall_2);

    }
}register__;

