//#include "xBase/xDefine.h"

//#include "xScheduler/xScheduler.h"

#include "rpc/myservice.grpc.pb.h"

#include "xRpc/xRpc.h"
#include "xEvent/xEvent.h"
#include "xService/xService.h"
#include "xEvent/xEventDispatcher.h"

#include "msg/login.pb.h"

#include "xNats/xNatsCall.h"




void TestEventCall_(EventScheduler* ptrEvent) {
    auto* pService = ptrEvent->m_pService;
    auto* pRequest = (PbMsg::LoginRequest*)(ptrEvent->m_ptrEvent->Request());
    auto* pResponse = (PbMsg::LoginResponse*)(ptrEvent->m_ptrEvent->Response());


    std::cout << "TestEventCall_ begin service id:" << pService->Id() << " index:" << pRequest->index() << std::endl;

    //std::this_thread::sleep_for(std::chrono::milliseconds(1));


    std::cout << "TestEventCall_ end service id:" << pService->Id() << " index:" << pRequest->index() << std::endl;
}

//
struct RegistEventDispatcher___
{
    RegistEventDispatcher___() {

        xEventDispatcher::getMe().RegistProtoCallBack(100, 102, TestEventCall_);

    }
}registerEvent__;

