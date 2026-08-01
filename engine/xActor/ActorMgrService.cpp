#include "ActorMgrService.h"
#include "ActorCreator.h"
#include "ActorMgr.h"
#include "xLog/xLog.h"

::grpc::Status ActorMgrServiceImpl::CreateActor(
    ::grpc::ServerContext* context,
    const ::actormgr::CreateActorRequest* request,
    ::actormgr::CreateActorReply* reply)
{
    int32 type = request->actor_type();
    int64 id   = request->actor_id();

    XLOG("[ActorMgrService] CreateActor: type=%d id=%lld", type, id);

    // 1. Check if this process hosts this actor type
    if (!CActorMgr::instance()->CanHostType(type)) {
        XWRN("[ActorMgrService] CreateActor: type=%d not hosted here", type);
        reply->set_success(false);
        reply->set_error_code(1);  // not hosted
        return ::grpc::Status::OK;
    }

    // 2. Create the C++ actor instance via prototype clone
    CActor* pActor = ActorCreator::Create(type, id);
    if (!pActor) {
        XERR("[ActorMgrService] CreateActor: ActorCreator::Create failed type=%d id=%lld", type, id);
        reply->set_success(false);
        reply->set_error_code(2);  // create failed
        return ::grpc::Status::OK;
    }

    // 3. Register with instance manager
    if (!CActorMgr::instance()->AddInstance(pActor)) {
        XERR("[ActorMgrService] CreateActor: AddInstance failed type=%d id=%lld", type, id);
        reply->set_success(false);
        reply->set_error_code(3);  // add failed
        return ::grpc::Status::OK;
    }

    // 4. Install actor environment (DB/Redis connections with ref counting)
    if (!CActorMgr::instance()->Install(type, id)) {
        XWRN("[ActorMgrService] CreateActor: Install failed type=%d id=%lld", type, id);
        CActorMgr::instance()->RemoveInstance(type, id);
        reply->set_success(false);
        reply->set_error_code(4);  // install failed
        return ::grpc::Status::OK;
    }

    reply->set_success(true);
    reply->set_error_code(0);
    XLOG("[ActorMgrService] CreateActor: success type=%d id=%lld", type, id);
    return ::grpc::Status::OK;
}

::grpc::Status ActorMgrServiceImpl::FindActor(
    ::grpc::ServerContext* context,
    const ::actormgr::FindActorRequest* request,
    ::actormgr::FindActorReply* reply)
{
    int32 type = request->actor_type();
    int64 id   = request->actor_id();

    CActor* pActor = CActorMgr::instance()->Find(type, id);
    if (pActor) {
        reply->set_found(true);
        reply->set_remote_type(static_cast<int32>(pActor->GetActorType()));
        XLOG("[ActorMgrService] FindActor: found type=%d id=%lld", type, id);
    } else {
        reply->set_found(false);
        reply->set_remote_type(0);
        XLOG("[ActorMgrService] FindActor: not found type=%d id=%lld", type, id);
    }
    return ::grpc::Status::OK;
}

::grpc::Status ActorMgrServiceImpl::UninstallActor(
    ::grpc::ServerContext* context,
    const ::actormgr::UninstallActorRequest* request,
    ::actormgr::UninstallActorReply* reply)
{
    int32 type = request->actor_type();
    int64 id   = request->actor_id();

    XLOG("[ActorMgrService] UninstallActor: type=%d id=%lld", type, id);

    // Uninstall actor (decrements connection ref counts, may release connections)
    if (!CActorMgr::instance()->UnInstall(type, id)) {
        XWRN("[ActorMgrService] UninstallActor: UnInstall failed type=%d id=%lld", type, id);
        reply->set_success(false);
        reply->set_error_code(1);
        return ::grpc::Status::OK;
    }

    // Remove from instance map and delete via ActorCreator
    ActorCreator::DeleteActor(type, id);

    reply->set_success(true);
    reply->set_error_code(0);
    XLOG("[ActorMgrService] UninstallActor: success type=%d id=%lld", type, id);
    return ::grpc::Status::OK;
}
