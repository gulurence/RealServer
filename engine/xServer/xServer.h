#pragma once

#include "xServerDefine.h"

#include "xNet/xNetDefine.h"

#include <google/protobuf/message.h>



// 进行数据加载存储更新
/*
 * 服务器进程实例 内部负责初始化 ServiceLancher 和 ActorRegister
 * 1.管理进程的初始化 卸载 
 * 2.ServiceLancher 管理 service 的初始化 
 * 3.ActorRegister 管理 actor 的注册卸载
 * 
 * 启动之后向redis注册 service 以及 actor 容器服务
 * 配置文件需要配置 ServerID,集群管理使用的 Local,Port,AuthKey
 * 在Redis中的信息为 ServerID,Local,Port,AuthKey,OnlineState,ServiceID,ActorID
 * 默认启动的Service为: log config redis nats
 */
class xServer
{
public:
    xServer(){}
    virtual ~xServer() {
    }

public:
    bool Init(){}
    bool Release(){}

public:
    bool LoadFromRedis(){}
    bool UpdateToRedis(){}
    bool RemoveFromRedis(){}

private:

};


int main() {

    // 1. 初始化 config
    // 2. 初始化 log
    // 3. 初始化 redis
    // 4. 初始化 nats
    // 5. 初始化 mysq链接 (可选)
    // 6. 初始化 timer (可选)
    // 
    // 7. 注册 server
    // 
    // 8. 接受 Actor 注册,注销 {日志管理器,Player,WorldMap,Activity... .. .}

 
    // actor 注册流程
    // 1.Nats -> ActorRegist Msg -> {Type,ID,}
    // 2.RedisLoad -> MysqlLoad(如果Redis不存在) -> CreateActor -> 1.注册Redis -> 2.注册Nats协议监听

    // Create -> Init -> Regist -> UnRegist
    // GateActor
    // LogActor
    // PlayerActor
    // LoginActor


    /*    
        GateActor ：

        CActor *pGate = new CGate(type_gate, id);
        pGate->Init(){
            1.启动ASIO监听客户端
            2.
    
        }
        pGate->Regist(){
            1.注册Redis
            2.注册Nats
    
        }
        pGate->UnRegist(){
            1.注销Nats
            2.注销Redis

        }

    
    */

    return 0;
}