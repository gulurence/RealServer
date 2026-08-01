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