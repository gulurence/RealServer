#pragma once

#include "xCommand.h"

#pragma pack(1)

struct ServerLoadCmd : public xCommand
{
    ServerLoadCmd(unsigned char p = 0) :xCommand(SERVER_LOAD_CMD, p) {}
};


#define LOAD_INFO_REQ_SLOAD_CMD 1
struct ServerLoadInfoReqCmd : public ServerLoadCmd
{
    ServerLoadInfoReqCmd() :ServerLoadCmd(LOAD_INFO_REQ_SLOAD_CMD) {}

    uint32 sguid = 0;
};


#define LOAD_INFO_RSP_SLOAD_CMD 2
struct ServerLoadInfoRspCmd : public ServerLoadCmd
{
    ServerLoadInfoRspCmd() :ServerLoadCmd(LOAD_INFO_RSP_SLOAD_CMD) {
    }

    struct LoadNode
    {
        SGUID_UINT32 sguid;
        uint32 cur_load_value = 0;
        uint32 warning_load_value = 0;
        uint32 full_load_value = 0;
    };

    // 鏈€澶?0涓瓙鑺傜偣
    int nodeCount = 0;
    LoadNode listData[0];
};


// 褰撹礋杞藉噺灏戠殑鏃跺€?涓诲姩鍑忓皯鑺傜偣鍏抽棴涔嬪墠閫氱煡璐熻浇绠＄悊鑺傜偣
// SLauncher 鍙戦€佺粰鐩磋繛鐨?涓績鑺傜偣
/*
 * 1. SLauncher -> Center [ -> Node ]
 * 3.
 */
#define LOAD_INFO_SERVER_REMOVE_CMD 3
struct ServerLoadRemoveNodeCmd : public ServerLoadCmd
{
    ServerLoadRemoveNodeCmd() :ServerLoadCmd(LOAD_INFO_SERVER_REMOVE_CMD) {
    }

    int sguid = 0;
};

#pragma pack()







