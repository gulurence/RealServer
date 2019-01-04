#include "ZoneServer.h"


class MyServer : public ZoneServer
{
public:
    MyServer(const char *t, const char *n) :ZoneServer(t, n) {
    }
    ~MyServer() {
    }

public:
    bool init() {
        // 会和当前进程建立连接的所有服务器
        setConnectServerType("TransferServer");
        setConnectServerType("RegServer");
        setConnectServerType("AllianceServer");


        // 配置文件初始化

        // TODO

        // 逻辑系统初始化

        // TODO

        return true;
    }

    virtual bool checkConnect() {
        // 需要自己主动连接的服务器
        bool ret = true;
        UInt32 st = time(NULL);


        if (!checkConnectedServer("TransferServer"))
            if (!connectServerByType("TransferServer"))
                ret = false;

        if (!checkConnectedServer("RegServer"))
            if (!connectServerByType("RegServer"))
                ret = false;


        return true;
    }

    virtual void v_closeServer(xNetProcessor *np) {
        if (!np)
            return;

        if (strcmp(np->name, "TransferServer") == 0) {
            // 断开逻辑处理

            // TODO 
        }

        // ... .. .
    }

    virtual bool v_processCmd(string serverName, xNetProcessor *np, unsigned char *buf, unsigned short len) {
        if (!np || !buf || !len)
            return false;

        np->put(buf, len);

        return true;
    }
};

int main(int argc, char *argv[]) {
    if (3 > argc) {
        XERR("main param count < 3 [dir,servertype,servername]");
        return 0;
    }

#ifndef _WINDOWS
    daemon(1, 1);
#endif


    MyServer *myServer = new MyServer(argv[1], argv[2]);
    myServer->run();
    return 0;
}























