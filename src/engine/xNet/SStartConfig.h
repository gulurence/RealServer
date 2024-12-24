#pragma once

#include "../xBase/xDefine.h"
#include "../xCommand/xCommand.h"
#include "../xBase/xSingleton.h"
#include "../xNet/ServerPathTool.h"
#include "../xNet/xServerMgrDataDef.h"

#define MECHINE_SMANAGER_SID 81
#define MECHINE_SLAUNCHER_SID 82

// 鏈嶅姟鍣ㄨ繘绋嬪惎鍔ㄩ厤缃?
// 杩欓噷鏄彁渚涚粰娓告垙杩涚▼浣跨敤鐨勯厤缃鐞? 
struct SServerStartConfig : public xSingleton<SServerStartConfig>
{
public:
    SServerStartConfig(){}
    ~SServerStartConfig(){}

private:
    // 鏄惁鏄墜鍔ㄥ惎鍔ㄧ殑 
    bool m_bIsManuallyRun = false;
    // 杩涚▼鍏ㄩ厤缃?
    ServerNodeFullConfigST mServerNodeFullConfig;

public:
    bool InitGame(int argc, char *argv[]);
    bool InitConfig(uint32 sguid);
    void PrintBaseConfig();
    bool ProcessAlreadyRunning(const std::string &sName, uint32 sguid);    
    void SetServerNodeFullConfigST(const ServerNodeFullConfigST &stNode) {
        mServerNodeFullConfig = stNode;
    }

    // 鐗╃悊鏈烘湇鍔″櫒鍏ㄩ厤缃?
    const ServerNodeFullConfigST &getConfig() { return mServerNodeFullConfig; }
    const ServerNodeBaseConfigST &getBaseConfig() {
        return mServerNodeFullConfig.base_config;
    }

    void SetManuallyRun(bool runmoudle) {
        m_bIsManuallyRun = runmoudle;
    }
    bool GetManuallyRun() {
        return m_bIsManuallyRun;
    }

public:
    uint32 GetLauncherSGUID();
    uint32 GetMID() {
        return mServerNodeFullConfig.base_config.mid;
    }
    uint32 GetServerVersion() {
        return mServerNodeFullConfig.base_config.version;
    }
    const std::string &GetServerName() {
        return mServerNodeFullConfig.base_config.sname;
    }
    uint32 GetServerSGUID() {
        return mServerNodeFullConfig.base_config.sguid.getSGUID();
    }
    uint32 GetServerSID() {
        return mServerNodeFullConfig.base_config.sguid.getServerID();
    }
    uint32 GetServerGroupID() {
        return mServerNodeFullConfig.base_config.sguid.getGroupID();
    }
    uint32 GetServerNodeID() {
        return mServerNodeFullConfig.base_config.sguid.getNodeID();
    }
    uint32 IsChildNodeServer(uint32 sguid) {
        SGUID_UINT32 tempID(sguid);
        // 鏈湇鍔″櫒涓轰腑蹇冭妭鐐?妫€鏌ユ湇鍔″櫒涓烘湰闆嗙兢鐨勫瓙鑺傜偣 
        if (GetServerNodeID() == 0 && tempID.getNodeID() > 0
            // 鍜屽瓙鑺傜偣鐩稿樊 CENTER_SERVER_BASE_SID 
            && GetServerSID() - CENTER_SERVER_BASE_SID == tempID.getServerID()
            && GetServerGroupID() == tempID.getGroupID()) {
            return true;
        }

        return false;
    }
public:
    bool isCenter() {
        return mServerNodeFullConfig.base_config.sguid.isSCenter();
    }
    uint32 isLoad() {
        return mServerNodeFullConfig.base_config.is_load;
    }
    uint32 isLoadMgr() {
        return mServerNodeFullConfig.base_config.is_load_mgr;
    }
    // 鏄惁鍙互閾炬帴鍒版湰鏈嶅姟鍣ㄨ繘绋?
    bool canConnect(uint32 sguid, uint32 mid){
        if (GetServerSGUID() == sguid) {
            return false;
        }        
        // 鏍稿績鑺傜偣鏃犳潯浠舵帴鏀跺瓙鑺傜偣 
        if (IsChildNodeServer(sguid)) {
            return true;
        }
        if (!isCenter()) {
            return false;
        }
        SGUID_UINT32 tempSguid(sguid);
        if (!tempSguid.isSCenter()) {
            return false;
        }
        // 閾炬帴鍏崇郴琛ㄤ腑瀹氫箟鐨勬湇鍔″櫒閾炬帴鍏崇郴 
        if (mServerNodeFullConfig.src_sid_map.find(tempSguid.getServerID()) != mServerNodeFullConfig.src_sid_map.end()) {
            return true;
        }
        // 閾炬帴 SLauncher 鐩稿悓鏈嶅姟鍣ㄧ殑涓嶅仛闄愬埗 
        if (GetServerSID() == MECHINE_SLAUNCHER_SID
            && mid == SServerStartConfig::getBaseConfig().mid) {
            return true;
        }
        if (GetServerSID() == MECHINE_SMANAGER_SID 
            && tempSguid.getServerID() == MECHINE_SLAUNCHER_SID) {
            return true;
        }
        return false;
    }

public:
    // need req connect game server config map
    const ServerNodeBaseConfigMap  &GetReqConnectServerMap() {
        return mServerNodeFullConfig.req_connects;
    }
    // need connect to datebases config map
    const DBConfigNodeMap &GetDBConnectMap() {
        return mServerNodeFullConfig.db_config_list;
    }
    // need connect to redis config map
    const RedisConfigNodeMap &GetRedisConnectMap() {
        return mServerNodeFullConfig.redis_config_list;
    }

public:
    const std::string &GetSGUIDFullString() {
        static std::string gStrSGUID;
        gStrSGUID.clear();

        char tempStr[128] = { 0 };
        sprintf(tempStr, "%s_G%d_N%d", mServerNodeFullConfig.base_config.sname.c_str(), GetServerGroupID(), GetServerNodeID());

        return gStrSGUID = tempStr;
    }
};









