//#include "xBase/xDefine.h"

//#include "xScheduler/xScheduler.h"

#include "rpc/myservice.grpc.pb.h"

#include "xRpc/xRpc.h"
#include "xEvent/xEvent.h"
#include "xService/xService.h"
#include "xEvent/xEventDispatcher.h"

#include "msg/login.pb.h"



// 协议处理函数 1
SchedulerTask TestEventCall_1(xService* pService, PBEventPtr msgPtr) {
    auto *pRequest = (PbMsg::LoginRequest*)(msgPtr->Request());
    auto *pResponse = (PbMsg::LoginResponse*)(msgPtr->Response());

    std::cout << "TestEventCall_1111 begin " << pService->Id() <<" index:"<< pRequest->index() << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(60));

    // 构造请求
    myservice::HelloRequest request;
    request.set_name("World");
    // 构造响应
    myservice::HelloReply reply;

    grpc::Status status;
    //GRPCRequestCall("", myservice, MyService, HelloRequest, HelloReply, SayHello, request, reply, status);

    struct CorutineContinue
    {
        myservice::HelloRequest& request_;
        myservice::HelloReply& reply_;
        grpc::Status& status_;
        bool await_ready() {
            return false;
        }
        void await_suspend(std::coroutine_handle<> h) {
            std::cout << "RPC request started, will take milliseconds." << std::endl;
            std::thread([h, this] {
                {
                    auto channel___ = CRpcService::getMe().GetChannel("test_rpc");
                    std::unique_ptr<myservice::MyService::Stub> stub___ = myservice::MyService::NewStub(channel___);
                    grpc::ClientContext context___;
                    request_.set_name("xxxxxxxxxxxx");
                    status_ = stub___->SayHello(&context___, request_, &reply_);
                    if (status_.ok()) {
                        std::cout << "Server response: " << reply_.message() << std::endl;
                    } else {
                        std::cout << "gRPC failed: " << status_.error_message() << std::endl;
                    }
                }
                h.resume();
                }).detach();
        }
        void await_resume() {
            std::cout << "RPC request completed." << std::endl;
        }
        void Return_Value() {}
    }; co_await CorutineContinue(request, reply, status);

    // 处理响应
    if (status.ok()) {
        std::cout << "Server response: " << reply.message() << std::endl;
    } else {
        std::cout << "gRPC failed: " << status.error_message() << std::endl;
    }

    std::cout << "TestEventCall_1111 end " << pService->Id() << std::endl;
}

SchedulerTask TestEventCall_2(xService* pService, PBEventPtr msgPtr) {
    auto* pRequest = (PbMsg::LoginRequest*)(msgPtr->Request());
    auto* pResponse = (PbMsg::LoginResponse*)(msgPtr->Response());


    std::cout << "TestEventCall_2222 begin " << pService->Id() << " index:" << pRequest->index() << std::endl;

    //std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // 构造请求
    myservice::HelloRequest request;
    request.set_name("World");
    // 构造响应
    myservice::HelloReply reply;

    grpc::Status status;
    //GRPCRequestCall("", myservice, MyService, HelloRequest, HelloReply, SayHello, request, reply, status);

    struct CorutineContinue
    {
        myservice::HelloRequest& request_; 
        myservice::HelloReply& reply_; 
        grpc::Status& status_; 
        bool await_ready() {
            return false; 
        }
        void await_suspend(std::coroutine_handle<> h) {
            std::cout << "RPC request started, will take milliseconds." << std::endl; 
            std::thread([h, this] {
                {
                    auto channel___ = CRpcService::getMe().GetChannel("test_rpc"); 
                    std::unique_ptr<myservice::MyService::Stub> stub___ = myservice::MyService::NewStub(channel___);
                    grpc::ClientContext context___;
                    request_.set_name("123123123");
                    status_ = stub___->SayHello(&context___, request_, &reply_);
                    if (status_.ok()) {
                        std::cout << "Server response: " << reply_.message() << std::endl; 
                    } else {
                        std::cout << "gRPC failed: " << status_.error_message() << std::endl; 
                    }
                }
                h.resume(); 
                }).detach(); 
        }
        void await_resume() {
            std::cout << "RPC request completed." << std::endl; 
        }
        void Return_Value() {}
    }; co_await CorutineContinue(request, reply, status);

    // 处理响应
    if (status.ok()) {
        std::cout << "Server response: " << reply.message() << std::endl;
    } else {
        std::cout << "gRPC failed: " << status.error_message() << std::endl;
    }


    //co_await EmptyCorutine{};

    std::cout << "TestEventCall_2222 end " << pService->Id() << " index:" << pRequest->index() << std::endl;
}

void TestEventCall_3(xService* pService, PBEventPtr msgPtr) {
    auto* pRequest = (PbMsg::LoginRequest*)(msgPtr->Request());
    auto* pResponse = (PbMsg::LoginResponse*)(msgPtr->Response());


    std::cout << "TestEventCall_3333 begin " << pService->Id() << " index:" << pRequest->index() << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));


    std::cout << "TestEventCall_3333 end " << pService->Id() << " index:" << pRequest->index() << std::endl;
}

//
struct RegistDispatcher___{
    RegistDispatcher___() {

        xEventDispatcher::getMe().RegistProtoCoroutineCallBack(100, 100, TestEventCall_1);
        xEventDispatcher::getMe().RegistProtoCoroutineCallBack(100, 101, TestEventCall_2);
        xEventDispatcher::getMe().RegistProtoCallBack(100, 102, TestEventCall_3);

    }
}register__;

