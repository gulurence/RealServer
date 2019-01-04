#include "ZoneServer.h"

string gStrCurServer;

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
        setConnectServerType("SuperServer");
        setConnectServerType("SessionServer");
        setConnectServerType("RecordServer");
        setConnectServerType("GateServer");
        setConnectServerType("LogServer");

//         if (!SceneConfig::getMe().init())
//             return false;
//         if (!MassiveConfig::getMe().loadConfig())
//             return false;
//         if (!EquipConfig::getMe().loadConfig())
//             return false;
//         if (!EquipConfig::getMe().loadStrConfig())
//             return false;
//         if (!CityConfig::getMe().loadConfig())
//             return false;

        return true;
    }

    virtual bool checkConnect() {
        // 需要自己主动连接的服务器
        bool ret = true;
        UInt32 st = time(NULL);

        if ("GateServer" == gStrCurServer) {

            if (!checkConnectedServer("SessionServer"))
                if (!connectServerByType("SessionServer"))
                    ret = false;
        }


//         if (!checkConnectedServer("SuperServer"))
//             if (!connectServerByType("SuperServer"))
//                 ret = false;
// 
//         if (!checkConnectedServer("SessionServer"))
//             if (!connectServerByType("SessionServer"))
//                 ret = false;
// 
//         if (!checkConnectedServer("RecordServer"))
//             if (!connectServerByType("RecordServer"))
//                 ret = false;
// 
//         if (!checkConnectedServer("LogServer"))
//             if (!connectServerByType("LogServer"))
//                 ret = false;

        //checkGateServer();

        return true;
    }

    virtual void v_closeServer(xNetProcessor *np) {
        if (!np)
            return;
        if (strcmp(np->name, "SessionServer") == 0) {
            server_stop();
        }
        else if (strcmp(np->name, "SuperServer") == 0) {

        }
        else if (strcmp(np->name, "RecordServer") == 0) {

        }
//         else if (strncmp(np->name, "GateServer", 10) == 0) {
//             pthread_rwlock_wrlock(&SceneUserManager::getMe().delgate_cirtical);
//             SceneUserManager::getMe().addOfflineGateCount();
//             pthread_rwlock_unlock(&SceneUserManager::getMe().delgate_cirtical);
//         }
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

    gStrCurServer = argv[1];

    MyServer *myServer = new MyServer(argv[1], argv[2]);
    myServer->run();
    return 0;
}


