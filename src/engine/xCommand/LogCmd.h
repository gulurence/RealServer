#pragma once

#include "xCommand.h"


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

        struct ObjLogData {
            uint64 accid;
            uint32 zoneid;
            uint32 time;
            uint64 charid;
            uint32 objID;
            char objName[MAX_NAMESIZE];
            uint32 changeNum;
            char log[MAX_NAMESIZE];
            //ItemGuid guid;
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

        struct EquipLogData {
            uint64 accid;
            uint32 zoneid;
            uint32 time;
            uint64 charid;
            uint32 equipID;
            char equipName[MAX_NAMESIZE];
            char log[32];
            //ItemGuid guid;
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
    }
}

#pragma pack()







