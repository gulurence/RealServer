#include "ZoneServer.h"
#include "xBalanceMgr.h"

#include "PressureTestCmd.h"


class MyServer : public ZoneServer
{
public:
    MyServer(const char *t, const char *n) : ZoneServer(t, n) {
    }
    ~MyServer() {
    }

public:
    bool init() {
        // ��͵�ǰ���̽������ӵ����з�����
        setConnectServerType("RegServer");
        setConnectServerType("GameServer");


        // �����ļ���ʼ��

        // TODO

        // �߼�ϵͳ��ʼ��

        // TODO

        return true;
    }

    virtual bool verifyServer(xNetProcessor *np, const char *t, const char *n) override {
        // ������������ӳɹ� ���븺���б�
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
                XERR("add server to balance error [%s,%s] !!!", t ? t : "", n ? n : "");
                return false;
            }

            return true;
        }

        return false;
    }

    virtual bool checkConnect() override {
        // ��Ҫ�Լ��������ӵķ�����
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

        // ɾ�������ؽڵ�
        if (!BalanceMgrInst::singleton()->removeBalanceNode(np)) {
            XERR("remove server to balance error [%s] !!!", np->m_arrcName);
        }

        if (strcmp(np->m_arrcName, "RegServer") == 0) {
            // �Ͽ��߼�����

            // TODO 

        }

        // ... .. .

    }

    virtual bool v_processCmd(string serverName, xNetProcessor *np, unsigned char *buf, unsigned short len) override {
        if (!np || !buf || !len)
            return false;

        xCommand *pCmd = (xCommand *)buf;

        // ������Ϣ
        if (PRESSURE_TEST_CMD == pCmd->cmd) {
            if (BASE_TEST_SYSCMD == pCmd->param) {
                basePressureTest(np, buf, len);
                return true;
            }
        }

        //���浽��Ϣ����
        np->put(buf, len);

        return true;
    }

public:
    void basePressureTest(xNetProcessor *np, unsigned char *buf, unsigned short len) {

        BasePressureTestSystemCmd *pMsg = (BasePressureTestSystemCmd *)buf;

        // ����Ѿ����յ�Server�� ���򷢸�����Ϣ��Server
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
    printf("���� �ж��ź� [%d] \n", s);
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
            myServer = new MyServer("LoginServer", optarg);
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
        XERR("�����в������� ��ָ�� -n ServerName");

    return 0;
}

