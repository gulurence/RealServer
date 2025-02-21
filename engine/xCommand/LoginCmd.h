#pragma once
#include "UserCmd.h"
#pragma pack(1)

namespace Cmd {
//    /************************* 闁谎嗩嚙缂嶅秹骞愰崶锔藉Б *****************************/
//    struct LoginCmd : public UserCmd
//    {
//        LoginCmd(uint8 p) :UserCmd(LOGIN_CMD, p) {
//        }
//    };
//
//    //client->regserver
//#define ACCPASS_REG_USER_CMD 1
//    struct AccPassRegUserCmd : public LoginCmd
//    {
//        AccPassRegUserCmd() : LoginCmd(ACCPASS_REG_USER_CMD) {
//            bzero(acc, sizeof(acc));
//            bzero(pass, sizeof(pass));
//            zoneID = 0;
//        }
//        char acc[MAX_NAMESIZE];
//        char pass[MAX_NAMESIZE];
//        uint32 zoneID;
//    };
//
//#define SYS_INFO_USER_CMD 2
//    struct SysInfoUserCmd : public LoginCmd
//    {
//        SysInfoUserCmd() : LoginCmd(SYS_INFO_USER_CMD) {
//            bzero(info, sizeof(info));
//        }
//        char info[MAX_NAMESIZE];
//    };
//
//    // regServer -> Client
//#define GATE_IP_USER_CMD 3
//    struct GateIPUserCmd : public LoginCmd
//    {
//        GateIPUserCmd() : LoginCmd(GATE_IP_USER_CMD) {
//            ip = 0;
//            port = 0;
//            len = 0;
//        }
//        uint32 ip;
//        int port;
//        uint16 len;
//        uint8 loginData[0];    //闁谎冾煼濡句即寮悧鍫濈ウ: sha1|accid|loginTime ...
//    };
//
//    struct LoginData
//    {
//        char sha1[SHA1_LEN];
//        uint64 accid;
//        uint32 loginTime;
//        uint32 zoneid;
//        uint16 addict;
//        char source[USER_SOURCE_LEN];
//        LoginData() {
//            bzero(sha1, sizeof(sha1));
//            accid = 0;
//            loginTime = 0;
//            zoneid = 0;
//            addict = 1;
//            bzero(source, sizeof(source));
//        }
//    };
//
//#define REG_GATE_USER_CMD 4
//    struct RegGateUserCmd : public LoginCmd
//    {
//        RegGateUserCmd() : LoginCmd(REG_GATE_USER_CMD) {
//            loginType = 0;
//            len = 0;
//        }
//        uint8 loginType;        // 闁谎冾煼濡炬壆鐚剧拠鑼偓?1: 閻庡箍鍨洪崺娑氱博? 2: 缂傚啯鍨块妴澶岀博?
//        //        char minloginType[MAX_BUFFER_LEN];//闁谎嗩嚙缂嶅秶鈧稒鍔曢崹搴ｇ尵?
//        uint16 len;
//        uint8 loginData[0];    //闁谎冾煼濡句即寮悧鍫濈ウ: sha1|accid|loginTime ...
//    };
//
//#define REG_ERR_USER_CMD 5
//    struct RegErrUserCmd : public LoginCmd
//    {
//        RegErrUserCmd() : LoginCmd(REG_ERR_USER_CMD) {
//            bzero(info, sizeof(info));
//            ret = ERR_NONE;
//        }
//        char info[USER_SOURCE_LEN];
//        ErrRet ret;
//    };
//
//    //GateWay to Client
//#define SNAPSHOT_USER_CMD 6
//    struct SnapShotUserCmd : public LoginCmd
//    {
//        uint8 ret;
//        uint64 createUID;
//        //SnapShotData data[MAX_ROLE_SIZE];
//        SnapShotUserCmd() : LoginCmd(SNAPSHOT_USER_CMD) {
//            createUID = 0;
//            ret = 0;
//        }
//    };
//
//    //client to //Session
//#define LOGIN_USER_CMD 7 
//    struct LoginUserCmd : public LoginCmd
//    {
//        uint64 id;
//        LoginUserCmd() : LoginCmd(LOGIN_USER_CMD) {
//            id = 0;
//        }
//    };
//
//    //gate to client,login succeed
//#define LOGIN_RESULT_USER_CMD 8
//    struct LoginResultUserCmd : public LoginCmd
//    {
//        LoginResultUserCmd() : LoginCmd(LOGIN_RESULT_USER_CMD) {
//            ret = 0;
//        }
//        uint8 ret;    //2闁告瑯鍨禍鎺撴交濞戞ê寮抽柛锔惧劋濞?
//    };
//
//    //闁告帗绋戠紓鎾舵喆閹烘洖顥?
//#define CREATE_CHAR_USER_CMD 9
//    struct CreateCharUserCmd : public LoginCmd
//    {
//        char name[MAX_ROLENAMESIZE]; // 闁告艾绉惰ⅷ
//        uint8 role_career; // 闁煎崬濂旂粭?
//        uint16 len;
//        uint8 loginData[0];    //闁谎冾煼濡句即寮悧鍫濈ウ: sha1|accid|loginTime ...
//        CreateCharUserCmd() : LoginCmd(CREATE_CHAR_USER_CMD) {
//            bzero(name, sizeof(name));
//            role_career = 0;
//            len = 0;
//        }
//    };
//
//    //闁告帞濞€濞呭海鎲撮幒鏇烆棌
//#define DELETE_CHAR_USER_CMD 10
//    struct DeleteCharUserCmd : public LoginCmd
//    {
//        uint64 id;
//        DeleteCharUserCmd() :LoginCmd(DELETE_CHAR_USER_CMD) {
//            id = 0;
//        }
//    };
//
//    enum
//    {
//        CREATE_ERROR_NAME_USED = 0,    //闂佹彃绉撮幃?
//        CREATE_ERROR_CAN_NOT_USE = 1,//闁告凹鍋呭﹢渚€寮箛鏂垮Τ閻犲洤绉甸惇?
//        CREATE_ERROR_HAVE_SPACE = 2,//闁告凹鍋呭﹢浣虹矚閻戞澹?
//    };
//
//    //闂佹彃绉撮幃?
//#define NAME_USED_CHAR_USER_CMD 11
//    struct NameUsedCharUserCmd : public LoginCmd
//    {
//        uint16 type;
//        NameUsedCharUserCmd() :LoginCmd(NAME_USED_CHAR_USER_CMD) {
//            type = 0;
//        }
//    };
//
//    //閻犲洭鏀遍惇鎷岀疀椤愩倕寮?
//#define REQ_SNAP_CHAR_USER_CMD 12
//    struct ReqSnapCharUserCmd : public LoginCmd
//    {
//        ReqSnapCharUserCmd() :LoginCmd(REQ_SNAP_CHAR_USER_CMD) {
//        }
//    };
//
//#define REQ_RANDOMNAME_USER_CMD 13
//    struct ReqRandomNameUserCmd : public LoginCmd
//    {
//        uint8 sex;
//        ReqRandomNameUserCmd() :
//            LoginCmd(REQ_RANDOMNAME_USER_CMD) {
//
//            sex = 0;
//        }
//    };
//
//#define RET_RANDOWNAME_USER_CMD 14
//    struct RetRandomNameUserCmd : public LoginCmd
//    {
//        uint8 ret;
//        char rolename[MAX_ROLENAMESIZE];
//
//        RetRandomNameUserCmd() :
//            LoginCmd(RET_RANDOWNAME_USER_CMD) {
//            ret = 0;
//            bzero(rolename, MAX_ROLENAMESIZE);
//        }
//    };
//
//#define LOADGAMEOK_USER_CMD 15
//    struct LoadGameOKCmd :public LoginCmd
//    {
//        char flagkey[MAX_NAMESIZE];
//        LoadGameOKCmd() :LoginCmd(LOADGAMEOK_USER_CMD) {
//            bzero(flagkey, MAX_NAMESIZE);
//        }
//    };
//#define LOGIN_GATE_USER_CMD 104
//    struct LoginGateUserCmd : public LoginCmd
//    {
//        uint64 accid;
//        LoginGateUserCmd() : LoginCmd(LOGIN_GATE_USER_CMD) {
//            accid = 0;
//        }
//    };
}

#pragma pack()







