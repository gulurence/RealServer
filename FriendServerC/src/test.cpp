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
        // ��͵�ǰ���̽������ӵ����з�����
        setConnectServerType("TransferServer");
        setConnectServerType("RegServer");
        setConnectServerType("AllianceServer");


        // �����ļ���ʼ��

        // TODO

        // �߼�ϵͳ��ʼ��

        // TODO

        return true;
    }

    virtual bool checkConnect() {
        // ��Ҫ�Լ��������ӵķ�����
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
            // �Ͽ��߼�����

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























