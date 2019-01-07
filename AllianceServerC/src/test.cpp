#include "ZoneServer.h"
#include "PressureTestCmd.h"


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
        //UInt32 st = time(NULL);


        if (!checkConnectedServer("TransferServer"))
            if (!connectServerByType("TransferServer"))
                ret = false;

        if (!checkConnectedServer("RegServer"))
            if (!connectServerByType("RegServer"))
                ret = false;

        return ret;
    }

    virtual void v_closeServer(xNetProcessor *np) {
        if (!np)
            return;

        if (strcmp(np->m_arrcName, "TransferServer") == 0) {
            // 服务器断开逻辑处理

            // TODO
        }

        // ... .. .

    }
    
    virtual bool v_processCmd(string serverName, xNetProcessor *np, unsigned char *buf, unsigned short len) override {
        if (!np || !buf || !len)
            return false;

        xCommand *pCmd = (xCommand *)buf;

        // 测试消息
        if (PRESSURE_TEST_CMD == pCmd->cmd) {
            if (BASE_TEST_SYSCMD == pCmd->param) {
                basePressureTest(np, buf, len);
                return true;
            }
        }

        //缓存到消息队列
        np->put(buf, len);

        return true;
            }

public:
    void basePressureTest(xNetProcessor *np, unsigned char *buf, unsigned short len) {

        BasePressureTestSystemCmd *pMsg = (BasePressureTestSystemCmd *)buf;

        // 如果已经是终点Server了 反向发给发消息的Server
        if (getThisServerTypeId() == pMsg->targetServerTypeId) {
            int32 tempServerTypeId = pMsg->srcServerTypeId;
            pMsg->srcServerTypeId = pMsg->targetServerTypeId;
            pMsg->targetServerTypeId = tempServerTypeId;
        }

        sendCmdToTransfer(buf, len);
    }
};

MyServer *myServer = nullptr;

#ifndef _WINDOWS
#include <signal.h>
void kill_handler(int s) {
    myServer->setServerState(xServer::SERVER_SAVE);
    myServer->server_stop();
    printf("捕获到 中断信号 [%d] \n", s);
}
#endif

int main(int argc, char *argv[]) {

#ifndef _WINDOWS

    signal(SIGTERM, kill_handler);
    signal(SIGSTOP, kill_handler);
    signal(SIGINT, kill_handler);

    int oc = -1;
    while ((oc = getopt(argc, argv, "dn:")) != -1) {
        switch (oc) {
        case 'd':
        {
            XLOG_SETSILENT;
            daemon(1, 1);
        }
        break;
        case 'n':
        {
            myServer = new MyServer("AllianceServerC", optarg);
        }
        break;
        case '?':
            break;
        }
    }

#else

    if (2 <= argc) {
        myServer = new MyServer("AllianceServer", argv[1]);
    }

#endif

    if (myServer) {

        myServer->run();

        SAFE_DELETE(myServer);
    } else
        XERR("命令行参数错误 请指定 -n ServerName");

    return 0;
}

