#include "ZoneServer.h"
#include "xBalanceMgr.h"


class MyServer : public ZoneServer
{
public:
    MyServer(const char *t, const char *n) : ZoneServer(t, n) {
    }
    ~MyServer() {
    }

public:
    bool init() {
        // 会和当前进程建立连接的所有服务器
        setConnectServerType("RegServer");
        setConnectServerType("GameServer");


        // 配置文件初始化

        // TODO

        // 逻辑系统初始化

        // TODO

        return true;
    }

    virtual bool verifyServer(xNetProcessor *np, const char *t, const char *n) override {
        // 如果服务器连接成功 加入负载列表
        if (ZoneServer::verifyServer(np, t, n)) {
            BalanceNodeST *pNode = new BalanceNodeST();
            pNode->eState = eBalanceNodeState_Disconnect;
            pNode->strServerType = t ? t : "";
            pNode->strServerName = n ? n : "";
            pNode->u32CurRunCount = 0;
            pNode->u32MaxRunCount = 500;
            pNode->pNet = (void*)np;
            if (!BalanceMgrInst::singleton()->addBalanceNode(pNode)) {
                delete pNode;
                XERR("add server to balance error [%s,%s] !!!",t?t:"" , n?n:"");
                return false;
            }

            return true;
        }

        return false;
    }

    virtual bool checkConnect() override {
        // 需要自己主动连接的服务器
        bool ret = true;
        //UInt32 st = time(NULL);
        

        if (!checkConnectedServer("RegServer"))
            if (!connectServerByType("RegServer"))
                ret = false;

        return ret;
    }

    virtual void v_closeServer(xNetProcessor *np) override {
        if (!np)
            return;

        // 删除掉负载节点
        if (!BalanceMgrInst::singleton()->removeBalanceNode(np)) {
            XERR("remove server to balance error [%s] !!!", np->name);
        }

        if (strcmp(np->name, "RegServer") == 0) {
            // 断开逻辑处理

            // TODO 

        }

        // ... .. .

    }

    virtual bool v_processCmd(string serverName, xNetProcessor *np, unsigned char *buf, unsigned short len) override {
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


