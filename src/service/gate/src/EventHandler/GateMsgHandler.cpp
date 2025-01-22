//#include "xBase/xDefine.h"

//#include "xScheduler/xScheduler.h"

#include "rpc/myservice.grpc.pb.h"

#include "xRpc/xRpc.h"
#include "xEvent/xEvent.h"
#include "xService/xService.h"
#include "xEvent/xEventDispatcher.h"

#include "msg/login.pb.h"



// 协议处理函数 1
SchedulerTask TestEventCall_1(EventScheduler* ptrEvent) {
    EventConroutineDefer __defer(ptrEvent->GetServiceScheduler());
    auto* pService = ptrEvent->m_pService;
    auto *pRequest = (PbMsg::LoginRequest*)(ptrEvent->m_ptrEvent->Request());
    auto *pResponse = (PbMsg::LoginResponse*)(ptrEvent->m_ptrEvent->Response());

    std::cout << "TestEventCall_1111 begin service id:" << pService->Id() <<" index:"<< pRequest->index() << std::endl ;

    std::this_thread::sleep_for(std::chrono::milliseconds(60));

    char pszLog[128] = { 0 };
    sprintf(pszLog,"111 fun Req ServiceId:%ld,Index:%d", pService->Id(), pRequest->index());
    // 构造请求
    myservice::HelloRequest request;
    request.set_name(pszLog);
    // 构造响应
    myservice::HelloReply reply;

    grpc::Status status;
    //GRPCRequestCall("test_rpc", myservice, MyService, HelloRequest, HelloReply, SayHello, pService, request, reply, status);

    struct CorutineContinue
    {
        EventScheduler* ptrEvent_;
        myservice::HelloRequest& request_;
        myservice::HelloReply& reply_;
        grpc::Status& status_;
        bool await_ready() {
            return false;
        }
        void await_suspend(std::coroutine_handle<> h) {
            std::cout << "RPC request started, will take milliseconds." << std::endl;
            std::thread([this, h] {
                EventScheduler* ptrEvent = ptrEvent_;
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
                // reset
                auto pScheduler = ptrEvent->GetServiceScheduler();
                pScheduler->SetSchedulerState(SchedulerStateType_Blocked_End);
                h.resume();
                }).detach();
        }
        void await_resume() {
            std::cout << "RPC request completed." << std::endl;
        }
        void Return_Value() {}
    }; co_await CorutineContinue(ptrEvent, request, reply, status);

    // 处理响应
    if (status.ok()) {
        std::cout << "Server response: " << reply.message() << std::endl;
    } else {
        std::cout << "gRPC failed: " << status.error_message() << std::endl;
    }

    std::cout << "TestEventCall_1111 end service id:" << pService->Id() << std::endl;
}

// 协议处理函数 2
SchedulerTask TestEventCall_2(EventScheduler* ptrEvent) {
    EventConroutineDefer __defer(ptrEvent->GetServiceScheduler());
    auto* pService = ptrEvent->m_pService;
    auto* pRequest = (PbMsg::LoginRequest*)(ptrEvent->m_ptrEvent->Request());
    auto* pResponse = (PbMsg::LoginResponse*)(ptrEvent->m_ptrEvent->Response());

    std::cout << "TestEventCall_222 begin service id:" << pService->Id() << " index:" << pRequest->index() << std::endl ;

    std::this_thread::sleep_for(std::chrono::milliseconds(60));

    char pszLog[128] = { 0 };
    sprintf(pszLog, "222 fun Req ServiceId:%ld,Index:%d", pService->Id(), pRequest->index());
    // 构造请求
    myservice::HelloRequest request;
    request.set_name(pszLog);
    // 构造响应
    myservice::HelloReply reply;

    grpc::Status status;
    //GRPCRequestCall("test_rpc", myservice, MyService, HelloRequest, HelloReply, SayHello, pService, request, reply, status);

    struct CorutineContinue
    {
        EventScheduler* ptrEvent_;
        myservice::HelloRequest& request_;
        myservice::HelloReply& reply_;
        grpc::Status& status_;
        bool await_ready() {
            return false;
        }
        void await_suspend(std::coroutine_handle<> h) {
            std::cout << "RPC request started, will take milliseconds." << std::endl;
            std::thread([this, h] {
                EventScheduler* ptrEvent = ptrEvent_;
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
                // reset
                auto pScheduler = ptrEvent->GetServiceScheduler();
                pScheduler->SetSchedulerState(SchedulerStateType_Blocked_End);
                h.resume();
                }).detach();
        }
        void await_resume() {
            std::cout << "RPC request completed." << std::endl;
        }
        void Return_Value() {}
    }; co_await CorutineContinue(ptrEvent, request, reply, status);

    // 处理响应
    if (status.ok()) {
        std::cout << "Server response: " << reply.message() << std::endl;
    } else {
        std::cout << "gRPC failed: " << status.error_message() << std::endl;
    }

    std::cout << "TestEventCall_222 end service id:" << pService->Id() << std::endl;
}

void TestEventCall_3(EventScheduler* ptrEvent) {
    auto* pService = ptrEvent->m_pService;
    auto* pRequest = (PbMsg::LoginRequest*)(ptrEvent->m_ptrEvent->Request());
    auto* pResponse = (PbMsg::LoginResponse*)(ptrEvent->m_ptrEvent->Response());


    std::cout << "TestEventCall_3333 begin service id:" << pService->Id() << " index:" << pRequest->index() << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(1));


    std::cout << "TestEventCall_3333 end service id:" << pService->Id() << " index:" << pRequest->index() << std::endl;
}

//
struct RegistDispatcher___{
    RegistDispatcher___() {

        xEventDispatcher::getMe().RegistProtoCoroutineCallBack(100, 100, TestEventCall_1);
        xEventDispatcher::getMe().RegistProtoCoroutineCallBack(100, 101, TestEventCall_2);
        xEventDispatcher::getMe().RegistProtoCallBack(100, 102, TestEventCall_3);

    }
}register__;

