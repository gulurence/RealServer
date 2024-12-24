#pragma once

#include "../xBase/xDefine.h"
#include "../xCommand/xCommand.h"

/* *******************************************************
 *
 *  璐熻矗鐩稿鐩綍鐨勭鐞嗚缃?
 *
 *
 */


// 杩涚▼瀹炰緥鐨勮繍琛岀洰褰?闆嗙兢鍜岃妭鐐笽D鏈夊叧
#define GAME_SERVER_RUN_CONFIG_BASE_PATH    "../../../../../run/"
// 娓告垙杩涚▼涓嶴Launcher鐩稿鐩綍
#define GAME_SERVER_BIN_DIR_FROM_SLAUNCHER  "../../../SGame/"
// run shell path
#define GAME_SERVER_RUN_DIR_FROM_SLAUNCHER  "../../SGame/"


// 鏈嶅姟鍣ㄦ枃浠剁洰褰曠鐞?
class ServerPathTool
{
public:
    ServerPathTool(){}
    ~ServerPathTool(){}
    
public:

    // 渚濇嵁SGUID鐢熸垚 鏈嶅姟鍣ㄧ殑鎵ц鐩綍
    // ../../../../../run/SGame/DataRecord/G1_N1
    // serverType - "SGame"鎴栬€?SServer"
    // serverName - "DataRecord" 绛?
    static std::string GetGServerRunDirPath(const std::string &serverName, uint32 gid, uint32 nid) {
        if (serverName.empty()) {
            return "";
        }

        char tempPath[256] = { 0 };
        sprintf(tempPath, "%sSGame/%s/G%d_N%d/", GAME_SERVER_RUN_CONFIG_BASE_PATH, serverName.c_str(), gid, nid);
        return std::string(tempPath);
    }
    
    // 鑾峰彇鐩稿浜嶴Launcher鐨勬父鎴忚繘绋嬬洰褰?
    // ../../../SGame/DataRecord/1.0001/
    // version - 10000涓哄崟浣嶇殑鐗堟湰鍙?
    // 鍙彁渚涚粰Linux浣跨敤
    static std::string GetGameServerBinDirBySLauncherPath(std::string serverName, uint32 version) {
        if (serverName.empty()) {
            return "";
        }

        char tempPath[256] = { 0 };
#ifndef _WIN32
        sprintf(tempPath, "../../../bin/linux/SGame/%s/%d.%04d/SG_%s", serverName.c_str(), version/10000, version%10000, serverName.c_str());
#endif
        return std::string(tempPath);
    }

    // 鑾峰彇鐩稿浜嶴Launcher鐨勬父鎴忚繘绋嬬洰褰?
    // ../../../SGame/DataRecord/GS_DataRecord
    static std::string GetGameServerRunFullPathBySLauncherPath(std::string serverName, uint32 gid, uint32 nid) {
        if (serverName.empty()) {
            return "";
        }

        char tempPath[256] = { 0 };
#ifdef _WIN32
        sprintf(tempPath, "%s%s/G%d_N%d/SG_%s.exe", GAME_SERVER_RUN_DIR_FROM_SLAUNCHER, serverName.c_str(), gid, nid, serverName.c_str());
#else
        sprintf(tempPath, "%s%s/G%d_N%d/SG_%s", GAME_SERVER_RUN_DIR_FROM_SLAUNCHER, serverName.c_str(), gid, nid, serverName.c_str());
#endif
        return std::string(tempPath);
    }


    // 鑾峰彇鐩稿浜嶴Launcher鐨勬父鎴忚繘绋嬬洰褰?
    // ../../../SGame/DataRecord/GS_DataRecord
    static std::string GetGameServerRunDirBySLauncherPath(std::string serverName, uint32 gid, uint32 nid) {
        if (serverName.empty()) {
            return "";
        }

        char tempPath[256] = { 0 };
        sprintf(tempPath, "%s%s/G%d_N%d/", GAME_SERVER_RUN_DIR_FROM_SLAUNCHER, serverName.c_str(), gid, nid);
        return std::string(tempPath);
    }
};









