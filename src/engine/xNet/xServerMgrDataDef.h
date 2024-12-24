#pragma once


#include "xCommand/xCommand.h"

//
#define Begin_Serialize_Struct() \
if (dataPos__ + (int)sizeof(short) > maxLen__) { return false; }\
short &struct_size__ = *(short*)&buffer__[dataPos__];\
struct_size__ = dataPos__ + sizeof(short);

#define End_Serialize_Struct() \
short s_size = struct_size__ - dataPos__ - sizeof(short);\
dataPos__ = struct_size__;\
struct_size__ = s_size;\
return true;

//
#define Begin_UnSerialize_Struct() \
short struct_size__ = 0;\
if (dataPos__ + (int)sizeof(short) > maxLen__) { return true; }\
struct_size__ = *(short*)&buffer__[dataPos__]; \
dataPos__ += sizeof(short); \
struct_size__ += dataPos__;

#define End_UnSerialize_Struct() \
dataPos__ = struct_size__;\
return true;


#define SerializeT(inData, T) {\
if (struct_size__ + (int)sizeof(T) > maxLen__) { return false; }\
*(T*)&buffer__[struct_size__] = inData;\
struct_size__ += sizeof(T);\
}

#define UnSerializeT(outData, T) {\
if (dataPos__ + (int)sizeof(T) > struct_size__) { return true; }\
outData = *(T*)&buffer__[dataPos__];\
dataPos__ += sizeof(T);\
}

#define SerializeString(inStr) {\
if (struct_size__ + (int)sizeof(short) > maxLen__) { return false; }\
*(short*)&buffer__[struct_size__] = (short)inStr.size();\
struct_size__ += sizeof(short);\
if (struct_size__ + (short)inStr.size() > maxLen__) { return false; }\
memcpy(&buffer__[struct_size__],inStr.c_str(),inStr.size());\
struct_size__ += (short)inStr.size();\
}

#define UnSerializeString(outStr) {\
if (dataPos__ + (int)sizeof(short) > struct_size__) { return false; }\
short string_size__ = *(short*)&buffer__[dataPos__];\
dataPos__ += sizeof(short);\
if (dataPos__ + string_size__ > struct_size__) { return true; }\
char tempString____[20000]={0};\
memcpy(tempString____, &buffer__[dataPos__], string_size__);\
outStr = tempString____;\
dataPos__ += string_size__;\
}


#pragma pack(1)

// 鍏变韩鍐呭瓨鐨勬暟鎹粨鏋?
struct SMemStruct
{
    // SGUID 鏈嶅姟鍣ㄨ繘绋嬪敮涓€鏍囪瘑
    uint32 u32SGUID = 0;
    // 杩涚▼鍚嶇О
    char szProcessName[MAX_NAMESIZE] = { 0 };
    // PID 鐢ㄤ簬鍚姩妫€鏌?
    uint64 u64PID = 0;
    // 閰嶇疆鏁版嵁闀垮害
    short pNodeFullConfigDataLen = 0;
    // ServerNodeFullConfigST
    char pNodeFullConfigData[0];

    SMemStruct &operator=(const SMemStruct &in) {
        if (this == &in) {
            return *this;
        }
        memcpy(this, &in, sizeof(SMemStruct));
        return *this;
    }
};

// Smanager鍩虹閰嶇疆 
struct SManagerConfig
{
    std::string ip;
    uint32 port = 0;
    std::string vkey;

    SManagerConfig &operator=(const SManagerConfig &in) {
        if (this == &in) {
            return *this;
        }

        ip = in.ip;
        port = port;
        vkey = in.vkey;

        return *this;
    }

    bool Serialize(char *buffer__, short &dataPos__, const int &maxLen__) {
        Begin_Serialize_Struct();
        SerializeString(ip);
        SerializeT(port, uint32);
        SerializeString(vkey);
        End_Serialize_Struct();
    }
    bool UnSerialize(const char *buffer__, short &dataPos__, const int &maxLen__) {
        Begin_UnSerialize_Struct();
        UnSerializeString(ip);
        UnSerializeT(port, uint32);
        UnSerializeString(vkey);
        End_UnSerialize_Struct();
    }
};
// 鎻愪緵缁橲Launcher浣跨敤鐨凷manager鐨勫惎鍔ㄩ厤缃?
struct SLauncherConfig : public SManagerConfig
{
    uint32 mid = 0;
    uint32 port = 0;

    SLauncherConfig &operator=(const SLauncherConfig &in) {
        if (this == &in) {
            return *this;
        }

        mid = in.mid;
        port = port;

        return *this;
    }
    bool Serialize(char *buffer__, short &dataPos__, const int &maxLen__) {
        Begin_Serialize_Struct();
        SerializeT(mid, uint32);
        SerializeT(port, uint32);
        End_Serialize_Struct();
    }
    bool UnSerialize(const char *buffer__, short &dataPos__, const int &maxLen__) {
        Begin_UnSerialize_Struct();
        UnSerializeT(mid, uint32);
        UnSerializeT(port, uint32);
        End_UnSerialize_Struct();
    }
};

// 鏁版嵁搴撻厤缃粨鏋?
struct DBConfigNodeST
{
    uint32 dbid = 0;    // DB ID 
    std::string title;        // 鏍囬鏍囪
    short max_connect_count = 1;
    std::string ip;
    uint32 port = 0;
    std::string user;
    std::string passwd;
    std::string dbname;

    void Printf() const {
        printf("--------DBConfigNodeST----------\n dbid:%d\n,title:%s\n,max_connect_count:%d\n ip:%s\n port:%d\n user:%s\n passwd:%s\n dbname:%s\n ------------------\n",
               dbid, title.c_str(), max_connect_count, ip.c_str(), port, user.c_str(), passwd.c_str(), dbname.c_str());
    }

    DBConfigNodeST &operator=(const DBConfigNodeST &in) {
        if (this == &in) {
            return *this;
        }

        dbid = in.dbid;    // DB ID 
        title = in.title;        // 鏍囬鏍囪
        max_connect_count = in.max_connect_count;
        ip = in.ip;
        port = in.port;
        user = in.user;
        passwd = in.passwd;
        dbname = in.dbname;

        return *this;
    }
    bool Serialize(char *buffer__, short &dataPos__, const int &maxLen__) {
        Begin_Serialize_Struct();
        SerializeT(dbid, uint32);
        SerializeString(title);
        SerializeT(max_connect_count, short);
        SerializeString(ip);
        SerializeT(port, uint32);
        SerializeString(user);
        SerializeString(passwd);
        SerializeString(dbname);
        End_Serialize_Struct();
    }
    bool UnSerialize(const char *buffer__, short &dataPos__, const int &maxLen__) {
        Begin_UnSerialize_Struct();
        UnSerializeT(dbid, uint32);
        UnSerializeString(title);
        UnSerializeT(max_connect_count, short);
        UnSerializeString(ip);
        UnSerializeT(port, uint32);
        UnSerializeString(user);
        UnSerializeString(passwd);
        UnSerializeString(dbname);
        End_UnSerialize_Struct();
    }
};
typedef std::map<uint32, DBConfigNodeST> DBConfigNodeMap;

// redis 閰嶇疆鏂囦欢
struct RedisConfigNodeST
{
    uint32 redisid = 0; // ID 
    std::string title;        // 鏍囬鏍囪
    std::string ip;
    uint32 port = 0;
    std::string user;
    std::string authkey;

    void Printf() const {
        printf("--------RedisConfigNodeST----------\n redisid:%d\n,title:%s\n,ip:%s\n port:%d\n user:%s\n authkey:%s\n ------------------\n",
               redisid, title.c_str(), ip.c_str(), port, user.c_str(), authkey.c_str());
    }

    RedisConfigNodeST &operator=(const RedisConfigNodeST &in) {
        if (this == &in) {
            return *this;
        }

        redisid = in.redisid; // ID 
        title = in.title;// 鏍囬鏍囪
        ip = in.ip;
        port = in.port;
        user = in.user;
        authkey = in.authkey;

        return *this;
    }
    bool Serialize(char *buffer__, short &dataPos__, const int &maxLen__) {
        Begin_Serialize_Struct();
        SerializeT(redisid, uint32);
        SerializeString(title);
        SerializeString(ip);
        SerializeT(port, uint32);
        SerializeString(user);
        SerializeString(authkey);
        End_Serialize_Struct();
    }
    bool UnSerialize(const char *buffer__, short &dataPos__, const int &maxLen__) {
        Begin_UnSerialize_Struct();
        UnSerializeT(redisid, uint32);
        UnSerializeString(title);
        UnSerializeString(ip);
        UnSerializeT(port, uint32);
        UnSerializeString(user);
        UnSerializeString(authkey);
        End_UnSerialize_Struct();
    }
};
typedef std::map<uint32, RedisConfigNodeST> RedisConfigNodeMap;

// 鏈嶅姟鍣ㄨ妭鐐归厤缃?
struct ServerNodeBaseConfigST
{
    SGUID_UINT32 sguid;
    uint32  version = 0;
    std::string  sname;
    std::string  ip;
    std::string  exip;
    uint32  port = 0;
    uint32  extport = 0;
    uint32  auto_run = 0;
    uint32  is_load = 0;// 鏄惁鏄礋杞藉潎琛¤妭鐐?
    uint32  is_load_mgr = 0;// 鏄惁绠＄悊璐熻浇鍧囪　鑺傜偣
    uint32  cur_group_load_max = 0;// 褰撳墠缇ょ粍鎵胯浇鏈€澶у€?
    uint32  mid = 0;
    std::string  vkey;

    void Printf() const {
        printf("------------------\n sname:%s\n version:%d\n ip:%s\n exip:%s\n port:%d\n extport:%d vkey:%s\n ------------------\n",
               /*sguid.getServerID(), sguid.getGroupID(), sguid.getNodeID(),*/
               sname.c_str(), version, ip.c_str(), exip.c_str(), port, extport, vkey.c_str());
    }

    ServerNodeBaseConfigST &operator=(const ServerNodeBaseConfigST &in) {
        if (this == &in) {
            return *this;
        }

        sguid = in.sguid;
        version = in.version;
        sname = in.sname;
        ip = in.ip;
        exip = in.exip;
        port = in.port;
        extport = in.extport;
        auto_run = in.auto_run;
        is_load = in.is_load;
        is_load_mgr = in.is_load_mgr;
        cur_group_load_max = in.cur_group_load_max;
        mid = in.mid;
        vkey = in.vkey;

        return *this;
    }
    bool Serialize(char *buffer__, short &dataPos__, const int &maxLen__) {
        Begin_Serialize_Struct();
        SerializeT(sguid.getSGUID(), uint32);
        SerializeT(version, uint32);
        SerializeString(sname);
        SerializeString(ip);
        SerializeString(exip);
        SerializeT(auto_run, uint32);
        SerializeT(is_load, uint32);
        SerializeT(is_load_mgr, uint32);
        SerializeT(cur_group_load_max, uint32);
        SerializeT(mid, uint32);
        SerializeString(vkey);
        SerializeT(port, uint32);
        SerializeT(extport, uint32);
        End_Serialize_Struct();
    }
    bool UnSerialize(const char *buffer__, short &dataPos__, const int &maxLen__) {
        Begin_UnSerialize_Struct();
        UnSerializeT(sguid, uint32);
        UnSerializeT(version, uint32);
        UnSerializeString(sname);
        UnSerializeString(ip);
        UnSerializeString(exip);
        UnSerializeT(auto_run, uint32);
        UnSerializeT(is_load, uint32);
        UnSerializeT(is_load_mgr, uint32);
        UnSerializeT(cur_group_load_max, uint32);
        UnSerializeT(mid, uint32);
        UnSerializeString(vkey);
        UnSerializeT(port, uint32);
        UnSerializeT(extport, uint32);
        End_UnSerialize_Struct();
    }
};
typedef std::map<uint32, ServerNodeBaseConfigST> ServerNodeBaseConfigMap;


// 鏈嶅姟鍣ㄨ礋杞介厤缃?
typedef std::map<uint32, std::pair<uint32,uint32> > ServerLoadConfigMap;

// 鏈嶅姟鍣ㄨ妭鐐瑰叏閰嶇疆
struct ServerNodeFullConfigST
{
    // 鍩虹淇℃伅
    ServerNodeBaseConfigST      base_config;
    // 鍗曠嫭鐨勯厤缃?
    ServerNodeBaseConfigMap     req_connects;
    // 鏈夐摼鎺ュ叧绯荤殑鏈嶅姟鍣╯id
    std::map<uint32, uint32>         src_sid_map;
    // 鏁版嵁搴撻摼鎺?
    DBConfigNodeMap             db_config_list;
    // Redis閾炬帴
    RedisConfigNodeMap          redis_config_list;
    // 鏈嶅姟鍣ㄨ礋杞介厤缃?
    ServerLoadConfigMap         server_load_map;

    ServerNodeFullConfigST &operator=(const ServerNodeFullConfigST &in) {
        if (this == &in) {
            return *this;
        }

        // 鍩虹淇℃伅
        base_config = in.base_config;
        // 鍗曠嫭鐨勯厤缃?
        req_connects = in.req_connects;
        // 浣滀负Target琚摼鎺ョ殑 鍙戣捣SGUID map
        src_sid_map = in.src_sid_map;
        // 鏁版嵁搴撻摼鎺?
        db_config_list = in.db_config_list;
        // Redis閾炬帴
        redis_config_list = in.redis_config_list;
        // 鏈嶅姟鍣ㄨ礋杞介厤缃?
        server_load_map = in.server_load_map;

        return *this;
    }
    void Printf() {
        printf(" ------------------------------------------------------------------------ \n");
        base_config.Printf();
        PrintReqConnectInfo();
        for (auto it : src_sid_map) {
            printf("src_sid_map %d,%d \n", it.first, it.second);
        }
        for (auto it : db_config_list) {
            it.second.Printf();
        }
        for (auto it : redis_config_list) {
            it.second.Printf();
        }
        for (auto it : server_load_map) {
            printf("server load [sid:%d,load:%d] \n", it.first, it.second.first);
        }
        printf(" ------------------------------------------------------------------------ \n");
    }
    void  PrintReqConnectInfo() const {
        printf(" PrintReqConnectInfo =====================  \n");
        for (auto it : req_connects) {
            it.second.Printf();
        }
    }
    bool Serialize(char *buffer__, short &dataPos__, const int &maxLen__) {
        Begin_Serialize_Struct();

        // ServerNodeBaseConfigST
        bool ret = base_config.Serialize(buffer__, struct_size__, maxLen__);
        if (!ret) {
            return false;
        }

        // ServerNodeBaseConfigST
        SerializeT((short)req_connects.size(), short);
        for (auto it : req_connects) {
            ret = it.second.Serialize(buffer__, struct_size__, maxLen__);
            if (!ret) {
                return false;
            }
        }

        // map<uint32, uint32>
        SerializeT((short)src_sid_map.size(), short);
        for (auto it : src_sid_map) {
            SerializeT(it.first, uint32);
        }

        // DBConfigNodeST
        SerializeT((short)db_config_list.size(), short);
        for (auto it : db_config_list) {
            ret = it.second.Serialize(buffer__, struct_size__, maxLen__);
            if (!ret) {
                return false;
            }
        }

        // ServerNodeBaseConfigST
        SerializeT((short)redis_config_list.size(), short);
        for (auto it : redis_config_list) {
            ret = it.second.Serialize(buffer__, struct_size__, maxLen__);
            if (!ret) {
                return false;
            }
        }

        // ServerLoad
        SerializeT((short)server_load_map.size(), short);
        for (auto it : server_load_map) {
            SerializeT((uint32)it.first, uint32);
            SerializeT((uint32)it.second.first, uint32);
            SerializeT((uint32)it.second.second, uint32);
        }

        End_Serialize_Struct();
    }
    bool UnSerialize(const char *buffer__, short &dataPos__, const int &maxLen__) {
        Begin_UnSerialize_Struct();

        // ServerNodeBaseConfigST
        bool ret = base_config.UnSerialize(buffer__, dataPos__, struct_size__);
        if (!ret) {
            return false;
        }

        // ServerNodeBaseConfigST
        short req_connects_size = 0;
        UnSerializeT(req_connects_size, short);
        for (short i = 0; i < req_connects_size; ++i) {
            ServerNodeBaseConfigST stData;
            ret = stData.UnSerialize(buffer__, dataPos__, struct_size__);
            if (!ret) {
                return false;
            }
            //req_connects[stData.sguid.getSGUID()] = stData;
        }

        // map<uint32, uint32>
        short target_sguid_size = 0;
        UnSerializeT(target_sguid_size, short);
        for (short i = 0; i < target_sguid_size; ++i) {
            uint32 sguid_ = 0;
            UnSerializeT(sguid_, uint32);
            src_sid_map[sguid_] = 1;
        }

        // DBConfigNodeST
        short db_connects_size = 0;
        UnSerializeT(db_connects_size, short);
        for (short i = 0; i < db_connects_size; ++i) {
            DBConfigNodeST stData;
            ret = stData.UnSerialize(buffer__, dataPos__, struct_size__);
            if (!ret) {
                return false;
            }
            db_config_list[stData.dbid] = stData;
        }

        // ServerNodeBaseConfigST
        short redis_size = 0;
        UnSerializeT(redis_size, short);
        for (short i = 0; i < redis_size; ++i) {
            RedisConfigNodeST stData;
            ret = stData.UnSerialize(buffer__, dataPos__, struct_size__);
            if (!ret) {
                return false;
            }
            redis_config_list[stData.redisid] = stData;
        }

        short sload_size = 0;
        UnSerializeT(sload_size, short);
        for (short i = 0; i < sload_size; ++i) {
            uint32 sid = 0;
            uint32 loadmin = 0;
            uint32 loadmax = 0;
            UnSerializeT(sid, uint32);
            UnSerializeT(loadmin, uint32);
            UnSerializeT(loadmax, uint32);
            server_load_map[sid] = std::make_pair(loadmin, loadmax);
        }

        End_UnSerialize_Struct();
    }
};
typedef std::map<uint32, ServerNodeFullConfigST> ServerNodeFullConfigSTMap;



// 鍗曚釜鐗╃悊鏈虹殑鎵€鏈夐厤缃枃浠?
struct MechineFullConfigST
{
    // 鍗忚鏁版嵁
    // 褰撳墠鐗╃悊鏈虹殑mid
    int32 mid = 0;
    // 褰撳墠閰嶇疆鐗堟湰
    int32 config_version = 0;
    // 鏈墿鐞嗘満闇€瑕佽繍琛岀殑鎵€鏈夋父鎴忚繘绋?
    ServerNodeFullConfigSTMap servers_config_map;

    MechineFullConfigST &operator=(const MechineFullConfigST &in) {
        if (this == &in) {
            return *this;
        }

        mid = in.mid;
        config_version = in.config_version;
        servers_config_map = in.servers_config_map;

        return *this;
    }
    bool Serialize(char *buffer__, short &dataPos__, const int &maxLen__) {
        Begin_Serialize_Struct();
        SerializeT(mid, int32);
        SerializeT(config_version, int32);

        SerializeT((short)servers_config_map.size(), short);
        for (auto it : servers_config_map) {
            if (!it.second.Serialize(buffer__, struct_size__, maxLen__)) {
                return false;
            }
        }

        End_Serialize_Struct();
    }
    bool UnSerialize(const char *buffer__, short &dataPos__, const int &maxLen__) {
        Begin_UnSerialize_Struct();
        UnSerializeT(mid, int32);
        UnSerializeT(config_version, int32);

        short sizeI = 0;
        UnSerializeT(sizeI, short);
        for (short i = 0; i < sizeI; ++i) {
            ServerNodeFullConfigST stServerNodeFullConfigST;
            stServerNodeFullConfigST.UnSerialize(buffer__, dataPos__, struct_size__);
            //servers_config_map[stServerNodeFullConfigST.base_config.sguid.getSGUID()] = stServerNodeFullConfigST;
        }

        End_UnSerialize_Struct();
    }
};


#pragma pack()




