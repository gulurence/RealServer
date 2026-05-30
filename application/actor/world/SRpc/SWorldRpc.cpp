#pragma once

#include "SWorldRpc.h"

#include <grpcpp/grpcpp.h>
#include <iostream>
#include <memory>
#include <string>

#include "rpc/myservice.grpc.pb.h"


class GreeterServiceImpl final : public myservice::MyService::Service {
    grpc::Status SayHello(grpc::ServerContext* context, const myservice::HelloRequest* request, myservice::HelloReply* reply) override {
        std::string prefix("CSWorldRpc recive Hello request");
        reply->set_message(prefix + request->name());
        return grpc::Status::OK;
    }
};

bool CSWorldRpc::Init() {

    SRpcService::getMe().RegistService(new GreeterServiceImpl());

    return true;
}

