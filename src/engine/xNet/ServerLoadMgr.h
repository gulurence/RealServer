#pragma once

#include "../xBase/xDefine.h"
#include "../xBase/xSingleton.h"
#include "../xBase/xThread.h"
#include "../xBase/xTime.h"
#include "../xCommand/ServerLoadCmd.h"


struct LoadNode
{
    // 涓婁竴娆＄殑鏇存柊鏃堕棿
    uint64 pre_update_tick = 0;
    // 鏈嶅姟鍣╯guid
    SGUID_UINT32 sguid;
    // 褰撳墠鎵胯浇鏁伴噺
    uint32 cur_load_value = 0;
    // 鎵胯浇闃堝€?
    uint32 warning_load_value = 0;
    uint32 full_load_value = 0;

    const LoadNode &operator=(const LoadNode &in) {
        if (this == &in) { return *this; }
        memcpy(this, &in, sizeof(LoadNode));
        return *this;
    }
};

// sguid - node info
typedef std::map<uint32, LoadNode> LoadNodeMap;
// sid - node map [鏈嶅姟鍣ㄧ被鍨?澶у巺/瀹跺洯///]
typedef std::map<uint32, LoadNodeMap> LoadNodeTypeMap;


/* **************************************************************
 *
 *  鍧囪　璐熻浇瑙勫垯锛?
 *  1.鐜╁鍒涘缓瑙掕壊鐨勬椂鍊欎細鍘婚€夋嫨涓€涓?DataRecord 杩涜鍒涘缓 銆愭瘡涓?DataRecord 璐熻浇30W涓处鎴凤紝浠roupID涓烘爣璁帮紝鍚庣画鍙互鍔ㄦ€佽皟鏁撮槇鍊硷紝渚嬪 gid=1鍒涘缓璐︽埛瓒呰繃闃堝€硷紝灏卞紑鍚痝id=2銆?
 *  2.鍏朵粬鎵€鏈夊崟浜虹帺娉曞叏灞€绯荤粺 渚濇嵁  DataRecord 鐨刧id 杩涜鏁版嵁搴撶殑娉ㄥ唽锛屼緥濡傜帺瀹禔璐︽埛鍦―ataRecord鐨刧id涓?锛屽垯鍦ㄥ崟浜虹帺娉曠郴缁熶腑鐨勬暟鎹簱涔熶細鍒涘缓鍦?gid=1 
 *  3.褰撴湇鍔″櫒鍚姩璧锋潵鐨勬椂鍊?瀛愯妭鐐逛細鍘婚摼鎺enter鑺傜偣锛孋enter鑺傜偣浼氬幓閾炬帴鐧诲綍鑺傜偣锛?閾炬帴鐨勫悓鏃朵細甯﹀叆鏄惁鏄礋杞借妭鐐圭殑鏍囪锛?
 *  4.濡傛灉鏄湁璐熻浇鑺傜偣鏍囪鐨勯摼鎺ヤ腑蹇冭妭鐐规垨鑰呯櫥褰曡妭鐐逛細鍙戣捣璐熻浇淇℃伅鐨勮姹傦紝瀵瑰簲鐨勬湇鍔″櫒鍒欓渶瑕佸洖澶嶈礋杞戒俊鎭€恠key-index-info銆?
 *  5.褰撻渶瑕佷互璐熻浇淇℃伅涓哄熀鍑嗛€夌潃鏈嶅姟鍣ㄧ殑鏃跺€欙紝灏变互璐熻浇淇℃伅杩涜鍒嗛厤
 *  6.鍒嗛厤鐨勬椂鍊欎細鐢ㄤ袱绾ey杩涜鍒嗛厤 涓€绾ey 琛ㄧず绯荤粺绫诲瀷锛屼緥濡傚鍥郴缁燂紝鍖归厤绯荤粺锛?浜岀骇key 琛ㄧず鐜╁鎵€鍦?DataRecord 瀵瑰簲鐨刬ndex锛屽鏋滀笉闇€瑕乮ndex鍒欎紶閫?1 
 *   
 *  
 * 
 *  
 *                                                
 *   [LoginGate1/LoginGate2/LoginGateN]  <-------  LobbyCenter[gid=-1]  <--------  [LobbyGate1<gid=-1>/LobbyGate2<gid=-1>/LobbyGateN<gid=-1>]
 *                                                        -
 *                                                         -----
 *                                                           -    -
 *                                                            -     -> [DataRecord1<gid=1>/DataRecord2<gid=2>/DataRecordN<gid=n>]
 *                                                           -
 *                                                             -
 *                                                              ->  [HomeGate1<gid=1>/HomeGate2<gid=1>/HomeGateN<gid=n>] -> [MatchServer1<gid=-1>/MatchServer2<gid=-1>/MatchServerN<gid=-1>]
 *
 * 
 */

class ZoneServer;
// 绠＄悊鏈嶅姟鍣ㄩ棿鐨勮礋杞芥暟鎹?
class ServerLoadMgr : public xThread, public xSingleton<ServerLoadMgr>
{
public:
    ServerLoadMgr();
    ~ServerLoadMgr();

private:
    xTimer m_stTimer;
    LoadNodeTypeMap m_mapLoadNodeType;

private:
    // 鏈嶅姟鍣ㄥ崟渚? 
    ZoneServer *m_pServer = nullptr;
    
public:
    bool Init(ZoneServer *pServer);

public:
    // 鑾峰彇璐熻浇淇℃伅 
    uint32 GetServerNodeCount(uint32 sid);
    const LoadNodeTypeMap &GetLoadNodeTypeMap() {
        return m_mapLoadNodeType;
    }

public:
    // 鑾峰彇涓€涓渶浣崇殑璐熻浇鑺傜偣 
    uint32 GetNiceLoadSGUID(SERVER_ROUTE_SID_DEF sid);
    // 鑾峰彇gid涓殑涓€涓礋杞借妭鐐?
    uint32 GetNiceLoadSGUIDByGID(uint32 sid, uint32 gid);

public:
    // 娣诲姞鍏锋湁璐熻浇绫诲瀷鐨勬湇鍔″櫒鑺傜偣 
    LoadNode &AddSLoad(uint32 sguid);
    // 鍙戣捣璇锋眰璐熻浇淇℃伅 
    void ReqLoadInfo(uint32 sguid);
    // 鏇存柊鏈嶅姟鍣ㄨ礋杞芥暟鎹?
    void UpdateNode(const ServerLoadInfoRspCmd *);

public:
    virtual void thread_proc();
};









