#include "main.h"

#include "nats/nats.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <string>


void publish_message() {
    natsStatus status;
    natsConnection* conn = nullptr;

    // 连接到 NATS 服务器
    status = natsConnection_ConnectTo(&conn, "nats://NCXQMLYCJKQW2R64GEUVUVHRG2G4I4TUFVXUX5HDOLDTYESS7NCKVIOR@192.168.1.78:4222");
    if (status != NATS_OK) {
        std::cerr << "Error connecting to NATS: " << natsStatus_GetText(status) << std::endl;
        return;
    }

    // 发布消息到 "test.subject"
    const char* subject = "test.subject";
    const char* message = "Hello, NATS from Publisher!";
    status = natsConnection_Publish(conn, subject, (const uint8_t*)message, strlen(message));
    if (status != NATS_OK) {
        std::cerr << "Error publishing message: " << natsStatus_GetText(status) << std::endl;
        return;
    }

    std::cout << "Message sent to subject '" << subject << "': " << message << std::endl;


    std::string response;
    auto s = natsConnection_PublishRequest(conn, "foo", "help", &response, 1000);
    if (s == NATS_OK) {
        std::cout << "收到响应: " << response << std::endl;
    } 
    // else if (s == NATS_NO_RESPONSE) {
    //    std::cout << "没有收到响应" << std::endl;
    //}
    else {
        std::cerr << "请求失败: " << natsStatus_GetText(s) << std::endl;
    }

    // 关闭连接
    natsConnection_Destroy(conn);
}



void message_handler(natsConnection* conn, natsSubscription* sub, natsMsg* msg, void* closure) {
    const char* subject = natsMsg_GetSubject(msg);
    const char* message = (const char*)natsMsg_GetData(msg);

    std::cout << "Received message on subject '" << subject << "': " << message << std::endl;

    // 释放消息
    natsMsg_Destroy(msg);
}

void subscribe_message() {
    natsStatus status;
    natsConnection* conn = nullptr;
    natsSubscription* sub = nullptr;

    // 连接到 NATS 服务器
    status = natsConnection_ConnectTo(&conn, "nats://NCXQMLYCJKQW2R64GEUVUVHRG2G4I4TUFVXUX5HDOLDTYESS7NCKVIOR@192.168.1.78:4222");
    if (status != NATS_OK) {
        std::cerr << "Error connecting to NATS: " << natsStatus_GetText(status) << std::endl;
        return;
    }

    // 订阅 "test.subject"
    const char* subject = "test.subject";
    status = natsConnection_Subscribe(&sub, conn, subject, message_handler, nullptr);
    if (status != NATS_OK) {
        std::cerr << "Error subscribing to subject '" << subject << "': " << natsStatus_GetText(status) << std::endl;
        return;
    }

    std::cout << "Subscribed to subject '" << subject << "'\n";

    // 保持程序运行，等待消息
    std::this_thread::sleep_for(std::chrono::minutes(1));

    // 取消订阅
    natsSubscription_Destroy(sub);
    natsConnection_Destroy(conn);
}

int nats_main(int argc, char* argv[]) {

    // 发布消息
    publish_message();
    // 订阅消息
    subscribe_message();

    return 0;
}





