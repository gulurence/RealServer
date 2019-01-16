#pragma once
#include "xCommand.h"
//#include "ItemStruct.h"

#pragma pack(1)

namespace Cmd {
namespace Log {
struct LogCmd: public xCommand {
    LogCmd(unsigned char p) :
            xCommand(LOG_CMD, p) {
    }
};
enum MONEYTYPE {
    MONEY_TYPE_GOLD = 1,
    MONEY_TYPE_BIND_GOLD,
    MONEY_TYPE_SILVER,
    MONEY_TYPE_BIND_SILVER,
    MONEY_TYPE_SKILLPOINT
};
enum MONEY_OP_TYPE {
    MONEY_OP_TYPE_ADD = 1, MONEY_OP_TYPE_DEL = 2,
};

//银子和金子
struct MoneyLogData {
    uint64 accid;
    uint32 zoneid;
    uint32 time;
    uint64 charid;
    uint16 level;
    uint16 moneyType;
    uint16 opType;
    uint16 count;
    uint64 num;
    uint64 remain;
    char reason[48];
    uint32 platformid;
    MoneyLogData() {
        bzero(this, sizeof(*this));
    }
};
#define MONEY_LOGCMD 1
struct MoneyLogCmd: public LogCmd {
    MoneyLogData data;
    MoneyLogCmd() :
            LogCmd(MONEY_LOGCMD) {
    }
};

//NPC商店购买道具
struct SaleRecordData {
    uint64 accid;
    uint32 zoneid;
    uint32 time;
    uint64 charid;
    uint16 moneyType;
    uint64 price;
    uint32 itemID;
    char itemName[MAX_NAMESIZE];
    uint32 itemNum;
    uint16 qual;
    uint16 level;
    uint32 platformid;
    SaleRecordData() {
        bzero(this, sizeof(*this));
    }
};
#define NPC_SHOP_LOGCMD 3
struct NpcShopLogCmd: public LogCmd {
    SaleRecordData data;
    NpcShopLogCmd() :
            LogCmd(NPC_SHOP_LOGCMD) {
    }
};

//道具使用日志
struct ObjLogData {
    uint64 accid;
    uint32 zoneid;
    uint32 time;
    uint64 charid;
    uint32 objID;
    char objName[MAX_NAMESIZE];
    uint32 changeNum;
    char log[MAX_NAMESIZE];
    ItemGuid guid;
    ObjLogData() {
        accid =  zoneid=0;
        time = 0;
        charid = 0;
        objID = 0;
        bzero(objName, sizeof(objName));
        changeNum = 0;
        bzero(log, sizeof(log));
    }
};
#define OBJ_LOGCMD 4
struct ObjLogCmd: public LogCmd {
    ObjLogData data;
    ObjLogCmd() :
            LogCmd(OBJ_LOGCMD) {
    }
};

//装备使用日志
struct EquipLogData {
    uint64 accid;
    uint32 zoneid;
    uint32 time;
    uint64 charid;
    uint32 equipID;
    char equipName[MAX_NAMESIZE];
    char log[32];
    ItemGuid guid;
    EquipLogData() {
        accid = 0;
        zoneid=0;
        time = 0;
        charid = 0;
        equipID = 0;
        bzero(equipName, sizeof(equipName));
        bzero(log, sizeof(log));
    }
};
#define EQUIP_LOGCMD 5
struct EquipLogCmd: public LogCmd {
    EquipLogData data;
    EquipLogCmd() :
            LogCmd(EQUIP_LOGCMD) {
    }
};

enum ITEM_USER_STAT_TYPE {
    ITEM_USER_STAT_TYPE_NULL = 0,                    //不需统计
    //产出
    ITEM_USER_STAT_TYPE_GET_QUEST = 1,            //任务获得
    ITEM_USER_STAT_TYPE_GET_PICKUP = 2,            //拾取
    ITEM_USER_STAT_TYPE_GET_QUESTMAP = 3,        //副本获得
    ITEM_USER_STAT_TYPE_GET_NPCSHOP = 4,        //npc商店购买
    ITEM_USER_STAT_TYPE_GET_COMPOSITE = 5,        //合成
    ITEM_USER_STAT_TYPE_GET_GIFT = 6,            //礼包获得
    ITEM_USER_STAT_TYPE_GET_GM = 7,                //GM获得
    ITEM_USER_STAT_TYPE_GET_USER = 8,            //和玩家交易获得
    ITEM_USER_STAT_TYPE_GET_MEDIA = 9,            //媒体推广
    ITEM_USER_STAT_TYPE_GET_MALL = 13,          //buy from mall
    ITEM_USER_STAT_TYPE_GET_CARD = 14,          //抽牌
    //消耗
    ITEM_USER_STAT_TYPE_USE_USE = 15,            //使用
    ITEM_USER_STAT_TYPE_USE_NPCSHOP = 16,        //出售npc商店
    ITEM_USER_STAT_TYPE_USE_TOUSER = 17,        //交易给玩家
    ITEM_USER_STAT_TYPE_USE_THROW = 18,            //丢弃
    ITEM_USER_STAT_TYPE_USE_DESTROY = 19,        //销毁
    ITEM_USER_STAT_TYPE_USE_COMPOSITE = 20,        //合成删除
    ITEM_USER_STAT_TYPE_USER_SALE = 21,            //出售
    ITEM_USER_STAT_TYPE_USER_MAKE = 22,            //打造
    ITEM_USER_STAT_TYPE_OUT_GEM = 23,            //宝石摘取
};
//道具使用统计
#define ITEM_USE_STAT_LOGCMD 6
struct ItemUseStatLogCmd: public LogCmd {
    uint64 accid;
    uint32 zoneid;
    uint64 charid;
    uint32 time;
    uint32 itemID;
    char itemName[MAX_NAMESIZE];
    uint32 num;
    uint8 qual;
    uint8 type;
    ItemUseStatLogCmd() :
            LogCmd(ITEM_USE_STAT_LOGCMD) {
        accid =zoneid= 0;
        charid=0;
        time = 0;
        itemID = 0;
        bzero(itemName, sizeof(itemName));
        num = 0;
        qual = 0;
        type = 0;
    }
};

struct TradeLogData {
    uint64 accid;
    uint32 zoneid;
    uint64 charid;
    uint32 time;
    char fromName[MAX_NAMESIZE];
    char toName[MAX_NAMESIZE];
    uint32 itemID;
    char itemName[MAX_NAMESIZE];
    uint32 itemNum;
    ItemGuid guid;
    uint64 money;
    uint64 tax;
    char log[MAX_NAMESIZE];
    TradeLogData() {
        bzero(this, sizeof(*this));
    }
};
#define TRADE_LOGCMD 7
struct TradeLogCmd: public LogCmd {
    TradeLogData data;
    TradeLogCmd() :
            LogCmd(TRADE_LOGCMD) {
    }
};

#define CHAT_LOGCMD 8
struct ChatLogCmd: public LogCmd {
    uint64 accid;
    uint32 zoneid;
    uint32 time;
    uint64 id;
    char name[MAX_NAMESIZE];
    uint64 toID;
    char toName[MAX_NAMESIZE];
    uint8 channel;
    uint8 len;
    char str[0];
    ChatLogCmd() :
            LogCmd(CHAT_LOGCMD) {
        accid =zoneid= 0;
        time = 0;
        id = 0;
        bzero(name, sizeof(name));
        toID = 0;
        bzero(toName, sizeof(toName));
        channel = 0;
        len = 0;
    }
};

//强制保存
#define FORCE_SAVE_LOGCMD 9 
struct ForceSaveLogCmd: public LogCmd {
    ForceSaveLogCmd() :
            LogCmd(FORCE_SAVE_LOGCMD) {
    }
};

//任务日志
#define QUEST_LOGCMD 10
struct QuestLogCmd: public LogCmd {
    uint64 accid;
    uint32 zoneid;
    uint64 charid;
    uint32 time;
    uint32 questid;
    uint8 type;        //0接任务,1完成任务,2注销任务
    QuestLogCmd() :
            LogCmd(QUEST_LOGCMD) {
        //bzero(this, sizeof(*this));
        accid=charid=zoneid=time=questid=type=0;
    }
};

#define LOGININFO_LOGCMD 11
struct LoginInfoLogCmd: public LogCmd {

    uint32 time;
    uint64 charid;
    LoginInfoLogCmd() :
            LogCmd(LOGININFO_LOGCMD) {
        time = charid = 0;
    }
};
#define GROWUP_LOGCMD 12
struct GrowupLogCmd: public LogCmd {
    uint64 accid;
    uint32 zoneid;
    uint32 time;
    uint64 charid;
    uint64 currentexp;
    uint64 addexp;
    uint8 type;
    uint8 islevelup;
    uint16 oldlevel;
    uint16 level;
    char name[48];
    char log[48];
    GrowupLogCmd() :
            LogCmd(GROWUP_LOGCMD) {
        accid=charid=currentexp=addexp=zoneid=time=oldlevel=level=type=islevelup=0;
    }
};

#define USERVISIT_LOGCMD 13
struct UserVisitLogCmd: public LogCmd {
    uint32 time;
    uint64 accid;        //平台ID
    uint32 zoneid;        //区ID
    uint32 ipaddr;        //玩家IP
    uint8 maxCVerion;        //客户端大版本
    char minCVersion[MAX_BUFFER_LEN];        //客户端小版本
    uint8 type;        //访问类型 1 加载资源完成 2创建角色完成 3进入游戏
    char flagkey[MAX_NAMESIZE];        //客户端标识
    UserVisitLogCmd() :
            LogCmd(USERVISIT_LOGCMD) {

    //    bzero(this, sizeof(*this));
        accid=time=zoneid=ipaddr=maxCVerion=type=0;
        bzero(minCVersion,MAX_BUFFER_LEN);
        bzero(flagkey,MAX_NAMESIZE);
    }
};


#define TOTALSCENEUSERNUM 14
struct TotalSceneUserNum: public LogCmd {
    uint32 time;
    uint32 zoneid;
    uint32 mapid;
    uint32 num;//玩家人数
    uint32 platformid;
    TotalSceneUserNum() :
            LogCmd(TOTALSCENEUSERNUM) {
        time=zoneid=mapid=num=platformid=0;
    }
};
#define USERACTIVE_LOGCMD 15
struct UserActiveCmd:public LogCmd{
    uint32 time;
    uint32 zoneid;
    uint32 accid;
    uint64 charid;
    uint8 islogin;
    uint32 onlinetime;
    UserActiveCmd():LogCmd(USERACTIVE_LOGCMD){
        charid=time=zoneid=accid=onlinetime=islogin=0;
    }
};
#define CREATECHAR_LOGCCMD 16
struct CreateCharLogCmd:public LogCmd{
    uint32 time;
    uint64 accid;
    uint64 charid;
    uint32 zoneid;
    uint16 sex;
    uint16 job;
    uint8 createOrdel;
    char name[MAX_NAMESIZE];
    CreateCharLogCmd():LogCmd(CREATECHAR_LOGCCMD){
        accid=charid=time=zoneid=sex=job=createOrdel=0;
        bzero(name,MAX_NAMESIZE);
    }
};

#define UNIONINFO_LOGCMD 17
struct UnionInfoLogCmd:public LogCmd{
    uint32 time;//操作时间
    uint32 zoneid;//区ID
    uint64 septID;//工会ID
    char name[MAX_NAMESIZE];//工会名
    uint8 handle;//1创建 2删除 3改名
    UnionInfoLogCmd():LogCmd(UNIONINFO_LOGCMD){
        septID=time=zoneid=handle=0;
        bzero(name,MAX_NAMESIZE);
    }
};
#define USERBASEINFO_LOGCMD 18
struct UserBaseInfoLogCmd:public LogCmd{
    uint32 time;//操作时间
    uint64 accid;//平台ID
    uint64 charid;//角色ID
    uint32 zoneid;//区ID
    char name[MAX_NAMESIZE];
    uint8 sex;//性别
    uint8 profession;//职业
    uint8 handle;//1创建 2删除 3改名
    UserBaseInfoLogCmd():LogCmd(USERBASEINFO_LOGCMD){
        accid=charid=time=zoneid=sex=profession=handle=0;

    }
};
#define VIPINFO_LOGCMD 19
struct VipInfoLogCmd:public LogCmd{
    uint32 time;//操作时间
    uint64 accid;//平台ID
    uint64 charid;//角色ID
    uint32 zoneid;//区ID
    uint16 viplevel;//vip等级 ,0非vip
    VipInfoLogCmd():LogCmd(VIPINFO_LOGCMD){
        accid=charid=time=zoneid=viplevel=0;
    }
};
#define SENDPOSTFORGM_LOGCMD 20
struct SendPostForGMLogCmd:public LogCmd{
    uint32 time;
    uint64 accid;
    uint32 zoneid;
    uint64 charid;
    char title[MAX_GMMAIL_TITLE_LEN];
    char content[MAX_GMMAIL_CONTENT_LEN];
    SendPostForGMLogCmd():LogCmd(SENDPOSTFORGM_LOGCMD){
        accid=charid=time=zoneid=0;
        bzero(title,MAX_GMMAIL_TITLE_LEN);
        bzero(content,MAX_GMMAIL_CONTENT_LEN);
    }
};

#define OPERATING_DB_LOGCMD 95
struct OperatingDBCmd:public LogCmd{
    uint32 time;
    uint64 accid;
    uint32 zoneid;
    uint64 charid;

    uint32 objtype;
    uint32 operatingtype;
    uint32 systemid;

    uint32 p1;
    uint32 p2;
    uint32 p3;
    char param[1024];

    OperatingDBCmd():LogCmd(OPERATING_DB_LOGCMD){
        accid=charid=time=zoneid=0;
        objtype=operatingtype=systemid=0;
        p1=p2=p3=0;
        bzero(param,1024);
    }
};
}
}

#pragma pack()
