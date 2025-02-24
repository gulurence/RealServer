//#include "xBase/xDefine.h"

//#include "xScheduler/xScheduler.h"

#include "rpc/myservice.grpc.pb.h"

#include "xRpc/xRpc.h"
#include "xEvent/xEvent.h"
#include "xService/xService.h"
#include "xEvent/xEventDispatcher.h"

#include "msg/login.pb.h"

#include "xNats/xNatsCall.h"


// 协议处理函数 1
SchedulerTask TestNatsCall_1(EventScheduler* ptrEvent) {
    EventConroutineDefer __defer(ptrEvent->GetServiceScheduler());
    auto* pService = ptrEvent->m_pService;
    auto* pRequest = (PbMsg::LoginRequest*)(ptrEvent->m_ptrEvent->Request());
    auto* pResponse = (PbMsg::LoginResponse*)(ptrEvent->m_ptrEvent->Response());

    std::cout << "TestEventCall_1111 begin service id:" << pService->Id() << " index:" << pRequest->index() << std::endl;

    //std::this_thread::sleep_for(std::chrono::milliseconds(60));

    char pszLog[128] = { 0 };
    sprintf(pszLog, "111 fun Req ServiceId:%ld,Index:%d", pService->Id(), pRequest->index());
    // 构造请求
    myservice::HelloRequest request;
    request.set_name(pszLog);
    // 构造响应
    myservice::HelloReply reply;

    int32 status = 0;
    NatsRequest("test_rpc.test", request, reply, ptrEvent, status);
    // 处理响应
    if (status == NATS_OK) {
        std::cout << "TestNatsCall response: " << status << std::endl;
    } else {
        std::cout << "TestNatsCall failed: " << status << std::endl;
    }

    std::cout << "TestEventCall_1111 end service id:" << pService->Id() << std::endl;
}


//
struct RegistNatsDispatcher___
{
    RegistNatsDispatcher___() {

        xEventDispatcher::getMe().RegistProtoCoroutineCallBack(100, 100, TestNatsCall_1);

    }
}registerNats__;

