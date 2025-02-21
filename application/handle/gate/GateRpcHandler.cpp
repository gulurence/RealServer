#include "xBase/xDefine.h"

//#include "xScheduler/xScheduler.h"

//#include "xRpc/xRpc.h"

//#include "rpc/myservice.grpc.pb.h"



//// 协议处理函数 1
//Task messagecall1(Player* player, const std::string& message) {
//    //std::cout << "Player " << player->id_ << " received message: " << message << std::endl;
//    std::string tmp = message;
//    std::cout << "+++messagecall 1 Logic begin for player " << player->id_ << " message: " << tmp << std::endl;
//    co_await RpcAwaiter{ std::chrono::milliseconds(100) };  // 模拟RPC延迟
//    std::cout << "---messagecall 1 Logic end for player " << player->id_ << " message: " << tmp << std::endl;
//}
//
//// 协议处理函数 2
//Task messagecall2(Player* player, const std::string& message) {
//    //std::cout << "Player " << player->id_ << " received message: " << message << std::endl;
//    std::string tmp = message;
//    std::cout << "+++messagecall 2 Logic begin for player " << player->id_ << " message: " << tmp << std::endl;
//    co_await RpcAwaiter{ std::chrono::milliseconds(100) };  // 模拟RPC延迟
//    std::cout << "---messagecall 2 Logic end for player " << player->id_ << " message: " << tmp << std::endl;
//}



// 实现 MyService 服务
//class MyServiceImpl final : public myservice::MyService::Service
//{
//public:
//    // 实现 SayHello RPC 方法
//    grpc::Status SayHello(grpc::ServerContext* context, const myservice::HelloRequest* request, myservice::HelloReply* reply) override {
//
//        //player1.postRequest([&]() {
//        //    return rpc_messagecall1(&player1, [&]() {
//        //        
//        //        
//        //    });
//        //});
//
//        // 构造一个回复消息
//        std::string prefix("Hello, ");
//        reply->set_message(prefix + request->name());
//
//        // 返回成功状态
//        return grpc::Status::OK;
//    }
//};
//
//
//int smain() {
//    // 创建 MyServiceImpl 的实例
//    MyServiceImpl service;
//
//    // 创建 ServerBuilder
//    grpc::ServerBuilder builder;
//
//    // 设置服务器监听地址（例如 localhost:50051）
//    builder.AddListeningPort("localhost:50051", grpc::InsecureServerCredentials());
//
//    // 注册服务
//    builder.RegisterService(&service);
//
//    // 构建并启动服务器
//    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
//
//    std::cout << "Server listening on localhost:50051" << std::endl;
//
//    // 等待服务器完成
//    server->Wait();
//
//    return 0;
//}



