#pragma once

#include "xBase/xDefine.h"
#include "xBase/xSingleton.h"
#include "xNatsDefine.h"
#include "nats/nats.h"


class CNatsConnect
{
public:
    CNatsConnect(natsConnection* pConnect):m_pConnect(pConnect){}
    ~CNatsConnect(){}

public:
    void Release() {
        natsConnection_Destroy(m_pConnect);
    }

public:
    natsStatus SendMsg(const char *pData, const uint32 &u32Len) {
        // 发布消息到 "test.subject"
        const char* subject = "test.subject";
        //const char* message = "Hello, NATS from Publisher!";
        auto status = natsConnection_Publish(m_pConnect, subject, (const uint8_t*)pData, u32Len);
        if (status != NATS_OK) {
            std::cerr << "Error publishing message: " << natsStatus_GetText(status) << std::endl;
            return status;
        }
        //std::cout << "Message sent to subject '" << subject << "': " << message << std::endl;
        return NATS_OK;
    }

    natsStatus RequestMsg(const std::string &strSubject, const char* pData, const uint32& u32Len, char* pOutData, uint32& u32OutLen, uint64 u16WaitTimeTick=1000) {
        // 发送请求并等待响应
        //const char* request_subject = "request.subject";
        //const char* request_message = "Hello, NATS server!";
        natsMsg* response_msg = nullptr;
        // 向指定主题发送请求，并等待响应
        auto status = natsConnection_Request(&response_msg, m_pConnect, strSubject.c_str(), (const uint8_t*)pData, u32Len, u16WaitTimeTick); // 等待最多1秒
        if (status != NATS_OK) {
            //std::cerr << "Error sending request: " << natsStatus_GetText(s) << std::endl;
            return status;
        }

        u32OutLen = natsMsg_GetDataLength(response_msg);

        // 打印响应内容
        const char* response_data = (const char*)natsMsg_GetData(response_msg);
        memcpy(pOutData, response_data, u32OutLen);

        //std::cout << "Received response: " << response_data << std::endl;

        // 清理资源
        natsMsg_Destroy(response_msg);
        return NATS_OK;
    }

public:
    natsConnection* m_pConnect = nullptr;
};
typedef std::map<uint32, CNatsConnect*> NatsConnectMap;





class CNatsClients : public xSingleton<CNatsClients>
{
public:
    CNatsClients(){}
    ~CNatsClients(){}

private:

public:
    bool Init(uint32 u32ConnectCount = 2) {
        m_u32ConnectCount = u32ConnectCount;
        for (int i = 0;i< m_u32ConnectCount;++i) {
            natsConnection* pConn = nullptr;
            // 连接到 NATS 服务器
            auto status = natsConnection_ConnectTo(&pConn, "nats://NCXQMLYCJKQW2R64GEUVUVHRG2G4I4TUFVXUX5HDOLDTYESS7NCKVIOR@192.168.1.78:4222");
            if (status != NATS_OK) {
                std::cerr << "Error connecting to NATS: " << natsStatus_GetText(status) << std::endl;

                // 释放链接
                Release();

                return false;
            }
            m_mapConnect[i] = new CNatsConnect(pConn);
        }
        return true;
    }

    void Release() {
        for (auto &it: m_mapConnect) {
            it.second->Release();
        }
        m_mapConnect.clear();
    }

    void SendMsg() {


    }

    CNatsConnect* GetNatsConnect() {
        return nullptr;
    }

private:
    uint32 m_u32ConnectCount = 0;

private:
    NatsConnectMap m_mapConnect;
};


// 处理请求并发送响应的回调函数
inline void requestHandler(natsConnection* nc, natsSubscription* sub, natsMsg* msg, void* closure) {
    const char* reply_subject = "reply.subject";
    // 获取请求消息内容
    const char* request_data = (const char*)natsMsg_GetData(msg);
    std::cout << "Received request: " << request_data << std::endl;


    // 准备响应消息
    const char* response_message = "Hello, NATS client!";
    // 发送响应消息
    natsStatus s = natsConnection_Publish(nc, reply_subject, (const uint8_t*)response_message, std::strlen(response_message));
    if (s != NATS_OK) {
        std::cerr << "Error sending response: " << natsStatus_GetText(s) << std::endl;
    }

    // 确认消息
    natsMsg_Destroy(msg);
}

class CNatsServer : xSingleton<CNatsServer>
{
public:
    CNatsServer();
    ~CNatsServer();

private:


public:
    bool Init() {
        natsStatus s;
        natsConnection* nc = nullptr;
        natsSubscription* sub = nullptr;

        // 连接到 NATS 服务器
        s = natsConnection_Connect(&nc, nullptr);
        if (s != NATS_OK) {
            std::cerr << "Error connecting to NATS server: " << natsStatus_GetText(s) << std::endl;
            return 1;
        }

        // 订阅请求主题
        const char* request_subject = "request.subject";
        s = natsConnection_Subscribe(&sub, nc, request_subject, requestHandler, nullptr);
        if (s != NATS_OK) {
            std::cerr << "Error subscribing to subject: " << natsStatus_GetText(s) << std::endl;
            return 1;
        }

        // 等待请求并处理响应
        std::cout << "Waiting for requests..." << std::endl;
        nats_Sleep(10000);  // 让服务器持续运行等待请求

        // 清理资源
        natsSubscription_Destroy(sub);
        natsConnection_Destroy(nc);
    }

    void Release() {
        for (auto& it : m_mapConnect) {
            it.second->Release();
        }
        m_mapConnect.clear();
    }

    void SendMsg() {


    }

private:
    uint32 m_u32ConnectCount = 0;

private:
    NatsConnectMap m_mapConnect;
};
