#pragma once
#include "xTools.h"
#include "xDefine.h"

//邮件类型
enum MAIL_TYPE
{
    MAIL_TYPE_PLAYER,        //玩家邮件
    MAIL_TYPE_SYSTEM,        //系统邮件
    MAIL_TYPE_SYSTEM_CHARGEMAIL,    //收费邮件返回的系统邮件
    MAIL_TYPE_AUCTION_SALE,        //拍卖行邮件
    MAIL_TYPE_AUCTION_BUY,
    MAIL_TYPE_PURCHASE_BUY,        //求购行邮件
    MAIL_TYPE_PURCHASE_SALE,
    MAIL_TYPE_RECHARGE,        //系统充值邮件
};
#define BUFFER_CMD(send, type) \
    char send##buf_3g[MAX_BUFSIZE];\
    bzero(send##buf_3g, sizeof(send##buf_3g));\
    type *send = (type *)send##buf_3g;\
    constructInPlace<type>(send)

#define BUFFER_CMD_SIZE(send, type, size) \
    char send##buf_3g[size];\
    bzero(send##buf_3g, sizeof(send##buf_3g));\
    type *send = (type *)send##buf_3g;\
    constructInPlace<type>(send)

#pragma pack(1)

#define MAX_CMD 255
//#define ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ (MAX_CMD)

#define SUPER_CMD 254//254
#define GATEWAY_CMD 253//253
#define RECORD_CMD 252//252
#define SESSION_CMD 251//251
#define SCENE_CMD 250//250
#define REG_CMD 249//249
#define SCENE_SESSION_CMD 248//248
#define SEPT_CMD 247
#define CITY_CMD 246
#define COUNTRY_CMD 245
#define LOG_CMD 244
#define STATISTICS_CMD 243
#define CASTLE_CMD 242
#define MANAGE_CMD 200        //GM后台管理命令
#define SERVICES_CMD 199 //内部服务接口
#define MAX_USER_CMD 155

//#define SUPER_CMD (MAX_CMD-1)//254
//#define GATEWAY_CMD (MAX_CMD-2)//253
//#define RECORD_CMD (MAX_CMD-3)//252
//#define SESSION_CMD (MAX_CMD-4)//251
//#define SCENE_CMD (MAX_CMD-5)//250
//#define REG_CMD (MAX_CMD-6)//249
//#define SCENE_SESSION_CMD (MAX_CMD-7)//248
//#define SEPT_CMD (MAX_CMD-8)
//#define CITY_CMD (MAX_CMD-9)
//#define COUNTRY_CMD (MAX_CMD-10)
//#define LOG_CMD (MAX_CMD-11)
//#define STATISTICS_CMD (MAX_CMD-12)
//#define CASTLE_CMD (MAX_CMD-13)
//#define MANAGE_CMD (MAX_CMD-55)        //GM后台管理命令
//#define MAX_USER_CMD (MAX_CMD-100)



struct xCommand
{
    UInt8 cmd;
    UInt8 param;

    xCommand(UInt8 c=0, UInt8 p=0):cmd(c),param(p){}
    bool is(UInt8 c){return cmd==c;}
    bool isUserCmd(){return cmd<=MAX_USER_CMD;}

    bool operator==(const xCommand &item) const
    {
        return (cmd==item.cmd) && (param==item.param);
    }
    bool operator<(const xCommand &item) const
    {
        if (cmd == item.cmd) return param>item.param;
        return (cmd > item.cmd);
    }
    const char *getMaxCmdName(){
        string result="";
        switch(cmd)
        {
        case 0:
            result="SYSTEM_CMD";
            break;
        case 255:
            result="MAX_CMD";
            break;
        case 254:
            result="SUPER_CMD";
            break;
        case 253:
            result="GATEWAY_CMD";
            break;
        case 252:
            result="RECORD_CMD";
            break;
        case 251:
            result="SESSION_CMD";
            break;
        case 250:
            result="SCENE_CMD";
            break;
        case 249:
            result="REG_CMD";
            break;
        case 248:
            result="SCENE_SESSION_CMD";
            break;
        case 247:
            result="SEPT_CMD";
            break;
        case 246:
            result="CITY_CMD";
            break;
        case 245:
            result="COUNTRY_CMD";
            break;
        case 244:
            result="LOG_CMD";
            break;
        case 243:
            result="STATISTICS_CMD";
            break;
        case 242:
            result="CASTLE_CMD";
            break;
        case 200:
            result="MANAGE_CMD";
            break;
        case 199:
            result="SERVICES_CMD";
            break;
        case 155:
            result="MAX_USER_CMD";
            break;
        default:
            result="NO FIND!!";
            break;
        }
        return result.c_str();
    }
};

enum ONE_LEVEL_INDEX_TYPE
{
    ONE_LEVEL_INDEX_TYPE_COUNTRY = 0,
    ONE_LEVEL_INDEX_TYPE_MAP = 1,
    ONE_LEVEL_INDEX_TYPE_TEAM = 2,
    ONE_LEVEL_INDEX_TYPE_GROUP = 3,        //聊天群
    ONE_LEVEL_INDEX_TYPE_SEPT = 4,        //家族
    ONE_LEVEL_INDEX_TYPE_CITY = 5,        //城市
    ONE_LEVEL_INDEX_TYPE_MAX,
};

enum TWO_LEVEL_INDEX_TYPE
{
    TWO_LEVEL_INDEX_TYPE_SCREEN = 0,
    TWO_LEVEL_INDEX_TYPE_MAX = 1,
};


enum AWARD_TYPE{
    AWARD_TYPE_EXP            = -1,
    AWARD_TYPE_MONEY        = -2,
    AWARD_TYPE_ZUANSHI        = -3,
    AWARD_TYPE_SKILL        = -4, // 技能点
    AWARD_TYPE_ESP_SPIRIT    = -5, // ESP魂


};

#define broadcaseTwoLevelIndexCmd(t, index, index2, cmd, len)\
{\
    UInt8 buf[MAX_BUFSIZE];\
    bzero(buf, sizeof(buf));\
    BroadcastTwoLevelIndexGatewayCmd *forward = (BroadcastTwoLevelIndexGatewayCmd *)buf;\
    constructInPlace<BroadcastTwoLevelIndexGatewayCmd>(forward);\
    forward->indexT = t;\
    forward->i = index;\
    forward->i2 = index2;\
    forward->len = len;\
    bcopy(cmd, forward->data, (UInt32)len);\
    thisServer->sendCmdToServer(forward, sizeof(BroadcastTwoLevelIndexGatewayCmd)+len, "GateServer");\
}

enum ErrRet
{
    ERR_NONE            = 0,                //未定义错误
    ERR_LOGIN_OVERTIME    = 1,        //登录超时
    ERR_RELOGIN            = 2,            //网关重复登录
    ERR_RELOGIN_DIFF_GATE = 3,        //从不同网关重复登录
    ERR_SESSION_CREATE_FAILD = 4,    //会话创建角色失败
    ERR_NOTIFY_SCENE = 5,            //未找到对应场景
    ERR_GET_USER_DATA = 6,            //读档失败
    ERR_DIFF_CLIENT = 7,                //客户端版本不一致
    ERR_FIND_GATE = 8,                //未找到网关
    ERR_SCENE_CREATE_FAILD = 9,    //场景创建角色失败
    ERR_REQ_DATA_FROM_RECORD = 10,//存档服务器未开启
    ERR_SET_USER_DATA_SCENE = 11,    //场景添加管理器失败
    ERR_ENTER_SCENE = 12,            //进入场景失败
    ERR_SHA1_ERROR = 13,                //sha1验证失败
    ERR_FORBID_REG = 14,                //被封号
    ERR_FORBID_IP=15,                    //被封IP
    ERR_SERVER_FULL=16,                //服务器爆满
};

#pragma pack()
