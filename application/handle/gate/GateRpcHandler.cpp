//#include "xBase/xDefine.h"

//#include "xScheduler/xScheduler.h"

#include "rpc/myservice.grpc.pb.h"

#include "xRpc/xRpc.h"
#include "xEvent/xEvent.h"
#include "xService/xService.h"
#include "xEvent/xEventDispatcher.h"

#include "msg/login.pb.h"


// 协议处理函数 2
SchedulerTask TestEventCall_2(EventScheduler* ptrEvent) {
    EventConroutineDefer __defer(ptrEvent->GetServiceScheduler());
    auto* pService = ptrEvent->m_pService;
    auto* pRequest = (PbMsg::LoginRequest*)(ptrEvent->m_ptrEvent->Request());
    auto* pResponse = (PbMsg::LoginResponse*)(ptrEvent->m_ptrEvent->Response());

    std::cout << "TestEventCall_222 begin service id:" << pService->Id() << " index:" << pRequest->index() << std::endl;

    //std::this_thread::sleep_for(std::chrono::milliseconds(60));

    char pszLog[128] = { 0 };
    sprintf(pszLog, "222 fun Req ServiceId:%ld,Index:%d", pService->Id(), pRequest->index());
    // 构造请求
    myservice::HelloRequest request;
    request.set_name(pszLog);
    // 构造响应
    myservice::HelloReply reply;

    grpc::Status status;
    GRPCRequestCall("test_rpc", myservice, MyService, HelloRequest, HelloReply, SayHello, ptrEvent, pService, request, reply, status);

    // 处理响应
    if (status.ok()) {
        std::cout << "Server response: " << reply.message() << std::endl;
    } else {
        std::cout << "gRPC failed: " << status.error_message() << std::endl;
    }

    std::cout << "TestEventCall_222 end service id:" << pService->Id() << std::endl;
}

//
struct RegistRpcDispatcher___
{
    RegistRpcDispatcher___() {

        xEventDispatcher::getMe().RegistProtoCoroutineCallBack(100, 101, TestEventCall_2);

    }
}registerRpc__;

