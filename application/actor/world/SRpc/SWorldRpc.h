#pragma once

#include "xService/xService.h"
#include "xActor/Actor.h"
#include "xRpc/xRpc.h"


#include "xDatabase/xDBConnPool.h"
#include "xRedis/RedisMgr.h"
#include "xRpc/xRpc.h"
#include "xNats/xNats.h"

class CSWorldRpc : public xSingleton<CSWorldRpc>, xThread
{
public:
    CSWorldRpc() {}
    ~CSWorldRpc(){}

private:


public:


public:
    virtual bool Init();

public:
    bool InitEnv_Database(int db_config) {

        DBConfigST stConfig;

        DBConnPool *pDBPool = new DBConnPool();

        // bool DBConnPool::init(const char *pHost, int iPort, const char *pDBName, const char *pUser, const char *pPassWd, int iPoolCount)
        pDBPool->init(stConfig.host.c_str(), stConfig.port, stConfig.name.c_str(), stConfig.user.c_str(), stConfig.passwd.c_str(), stConfig.poolCount);

        return true;
    }
    bool InitEnv_Redis(int redis_config) {
        std::string key = "main_cluster";
        std::string url = "tcp://127.0.0.1:7000";
        CRedisClusterMgr::getMe().ConnectToCluster(key, url);

        return true;
    }
    bool InitEnv_GrpcServer(int grpc_config) {

        int32 port = 6231;
        SRpcService::getMe().Start(port);

        return true;
    }
    bool InitEnv_GrpcClient(int grpc_client_config) {

        int32 port = 6231;
        int32 poolCount = 4;
        std::string ipAddr = "192.168.1.1";
        std::string serverName = "123";

        CRpcService::getMe().ConnectToLocalServer(serverName, poolCount, port);
        CRpcService::getMe().ConnectToServer(serverName, poolCount, ipAddr, port);

        auto channel = CRpcService::getMe().GetChannel(serverName);
        return true;
    }
    bool InitEnv_Nats(int nats_config) {

        std::string url = "";
        int32 poolCount = 4;
        NatsConnPool::getMe().Init(url, poolCount);
    
        auto connect = NatsConnPool::getMe().getConnection();


        return true;
    }

public:
    virtual bool ApplicationInit() {

                

        return true;
    }

    virtual bool Install() {

        // 数据库 redis grpcserver grpcclient nats 

        return true;
    }

    virtual bool UnInstall() {

        return true;
    }
};
