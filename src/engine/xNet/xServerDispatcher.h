#pragma once

/*
===============================================================================

  xServerDispatcher

  1.Between Servers Send Msg Interface.
  2.Define Servers Type Class.
  3.Make Msg Package And Send To Other Server

  author:lurence.gu
===============================================================================
*/

#include "../xBase/xDefine.h"
#include "../xCommand/xCommand.h"
#include "../xCommand/ManageCmd.h"
#include "../xCommand/xServerRouteDefine.h"
#include "../xNet/xNetProcessor.h"
//#include "../xServer/ZoneServer.h"


/*
    鐩爣鏈嶅姟鍣?鍙戦€佹帴鍙?绫诲皝瑁呭畯
    1.鐢ㄤ簬灏佽瀹氫箟鐩爣鏈嶅姟鍣ㄧ殑鍙戦€佹帴鍙?
    
    浣跨敤鏂规硶锛?
        1.瀹氫箟鐩爣鏈嶅姟鍣ㄧ殑鍙戦€佹帴鍙?
            - DEF_SERVER_MSG_ROUTE_CLASS(LoginGate);// 瀹氫箟鍙戦€佺粰鐧诲綍缃戝叧鐨勫彂閫佺被

        2.缁ф壙鍙戦€佺被
            - class LobbyGateUser : public SDB_CLASS(LoginGate) ... // 澶у巺缃戝叧鐜╁绫?缁ф壙鍙戦€佺粰鐧诲綍缃戝叧鐨勬秷鎭彂閫佺被
            
        3.璁剧疆鐩爣鐧诲綍缃戝叧鐨凷GUID
            - pLobbyGateUser->SetLoginGateSUID(65798);

        4.鍚戣缃ソ鐨勭櫥褰曠綉鍏冲彂閫佹秷鎭?
            - PBLobbyGateUserGameDataReqCmd reqMsg;
            - ... .. .
            - TO_BUFFER_SEND_EX(reqMsg, pLobbyGateUser, SendToLoginGate);

        5.鐩爣鏈嶅姟鍣ㄧ洃鍚鐞嗘秷鎭?
            - bool doUserLobbyLogoutCmd(uint32 sguid, xNetProcessor *pNetProcess, const ::google::protobuf::Message *pMsg) {
            -    if (!pMsg) {
            -        XERR("doLoginGateLobbyGateRspCmd !pMsg");
            -        return true;
            -     }
            -     PBUserLobbyLogoutCmd *pCmd = (PBUserLobbyLogoutCmd *)pMsg;
            -     AccountStatusCheckMgr::getMe().UserLobbyLogout(pCmd->uid());
            -     return true;
            - }
            - 
            - // 鍗忚澶勭悊娉ㄥ唽
            - REGIST_MSG_CALLBACK_BEGIN(LoginGateCmd)
            - REGIST_PROTO_MSG_CALLBACK(CMD_RUN_THREAD_NET, PBUserLobbyLogoutCmd, doUserLobbyLogoutCmd);
            - REGIST_MSG_CALLBACK_END(LoginGateCmd)
*/
#define DEF_SERVER_MSG_ROUTE_CLASS(TARGET_SNAME) \
class x##TARGET_SNAME##MsgRoute{\
public:\
    x##TARGET_SNAME##MsgRoute(){}\
    ~x##TARGET_SNAME##MsgRoute(){}\
    void Set##TARGET_SNAME##SUID(uint32 u32UID){\
        m_u32UID = u32UID;\
    }\
    uint32 Get##TARGET_SNAME##SUID(){\
        return m_u32UID;\
    }\
    void BroadcastTo##TARGET_SNAME(SERVER_ROUTE_SID_DEF enTargetSType, bool bToChild, const char *buffer, int len){\
        xServerDispatcher::BroadcastMsgToServer(enTargetSType, bToChild, buffer, len, nullptr);\
    }\
    void SendTo##TARGET_SNAME(uint64 u64TargetUID, const char *buffer, int len){\
        xServerDispatcher::SendMsgToServer(u64TargetUID, buffer, len);\
    }\
    void SendTo##TARGET_SNAME(const char *buffer, int len){\
        xServerDispatcher::SendMsgToServer(m_u32UID, buffer, len);\
    }\
private:\
    uint32 m_u32UID = 0;\
};


// SDB_CLASS = Server Dispatcher Base Class
#define SDB_CLASS(TARGET_SNAME) x##TARGET_SNAME##MsgRoute


// 璺敱瑙勫垯 渚嬪 Lobby_Gate_G1_N1
// 绗竴娆℃煡鎵?Lobby_Gate_G1_N1    銆愰泦缇よ妭鐐广€?
// 绗簩娆℃煡鎵?Lobby_Gate_G1        銆愰泦缇D瀵瑰簲鐨勯泦缇や腑蹇冩湇鍔″櫒銆?
// 绗笁娆℃煡鎵?Lobby_Gate        銆愭墍鏈夐泦缇や腑蹇冩湇鍔″櫒銆?
class xServerDispatcher
{
public:
    //static ZoneServer *m_pZoneServer;
    
public:
    /* 
     * 鑾峰彇鏈嶅姟鍣ㄩ摼鎺ユ寚閽?鑾峰彇閫昏緫椤哄簭
     *
     * 1.鐩存帴鏌ユ壘鐩爣鏈嶅姟鍣↖D瀵瑰簲鐨?閾炬帴
     * 2.濡傛灉uid瀵瑰簲鐨勬槸涓績鑺傜偣 鑾峰彇浠栦滑鐨勫瓙 閾炬帴
     * 3.鑾峰彇uid瀵瑰簲鐨勪腑蹇冭妭鐐?
     */
    //static ServerClient *GetNetConnect(const uint32 &targetSUID);
    
    /*
     * 渚濇嵁鐩爣鏈嶅姟鍣ㄧ被鍨婭D鑾峰彇閾炬帴鍒楄〃
     *
     * 1.鑾峰彇鏈嶅姟鍣ㄧ被鍨嬬浉鍚岀殑閾炬帴鍒楄〃
     */
    // targetType    - 鐩爣鏈嶅姟鍣ㄧ被鍨?
    // bNeedCenter    - 涓績鑺傜偣鏄惁涓€璧疯幏鍙?杩樻槸鍙瀛愯妭鐐?
    // outList        - 杩斿洖鏌ユ壘鍒扮殑閾炬帴鍒楄〃
    // return 鏌ユ壘鍒扮殑鏁伴噺
    //static int GetNetConnectListSameAsSType(const SERVER_ROUTE_SID_DEF &targetType, bool bNeedCenter, std::list<ServerClient*> &outList);

public:
    /*
     * 鍙戦€佹秷鎭粰鎸囧畾SGUID鐨勬湇鍔″櫒 [绮剧‘鐨勫彂閫佺粰SGUID鐩稿悓鐨勬湇鍔″櫒杩涚▼]
     *
     * 1.鐩存帴鑾峰彇SGUID瀵瑰簲鐨勬湇鍔″櫒閾炬帴骞跺彂閫?
     * 2.濡傛灉 1 娌℃湁鎵惧埌鐩存帴浣跨敤骞挎挱鎺ュ彛缁欏叾浠栨湇鍔″櫒杩涜鍐嶆骞挎挱
     */
    static bool SendMsgToServer(uint32 targetSUID, const char *buffer, int len);

    // 骞挎挱鍙戦€佹秷鎭?缁欒嚜宸遍摼鎺ョ殑鎵€鏈夋湇鍔″櫒 [骞挎挱缁欎竴绫绘湇鍔″櫒 娉ㄦ剰灏戠敤瀛愯妭鐐圭殑鎯呭喌 浼氬箍鎾粰鎵€鏈夌殑瀛愯妭鐐筣
    // targetSUIDType - 鏈嶅姟鍣ㄧ被鍨?
    // srcUID 鍙戣捣鑰呯殑uid鐜╁鍞竴id
    // targetUID 鎺ユ敹鑰呯殑uid鐜╁鍞竴id
    // bToChild 鎺ユ敹鑰呯殑uid鐜╁鍞竴id
    static bool BroadcastMsgToServer(SERVER_ROUTE_SID_DEF targetSUIDType, bool bToChild, const char *buffer, int len, xNetProcessor *ignoreProcess = nullptr);

    // 鐩存帴鍙戦€佺粰鎵€鏈夌殑闄gnorSGUID浠ュ鐨勯摼鎺?
    static bool BroadcastMsgToServer(const char *buffer, int len, const RouteInfo *preInfo);
    
public:
    // 鎷嗚В 璺敱鍖?
    // return [true - 涓浆娑堟伅 涓嶈兘缁х画浼犻€掍簡, false 娑堟伅闇€瑕佺户缁矾鐢盷
    static bool OnMsg(CMD_RUN_THREAD_TYPE runThreadType, xNetProcessor *pNetProcess, const char *buffer, int len);    
};


// 瀹氫箟鎵€鏈夋湇鍔″櫒鐨勮矾鐢?鍩虹被

// 闆嗙兢 绠＄悊鍣ㄧ殑璺敱鍣?
DEF_SERVER_MSG_ROUTE_CLASS(SManager);
// 鏈湴绋?搴忕鐞嗗櫒鐨勮矾鐢卞櫒 
DEF_SERVER_MSG_ROUTE_CLASS(SLauncher);
// 鏁版嵁涓績 绠＄悊鍣ㄧ殑璺敱鍣?
DEF_SERVER_MSG_ROUTE_CLASS(DataRecord);
// 鐧诲綍缃戝叧 绠＄悊鍣ㄧ殑璺敱鍣?
DEF_SERVER_MSG_ROUTE_CLASS(LoginGate);
// 鐧诲綍璐熻浇鍣?绠＄悊鍣ㄧ殑璺敱鍣?
DEF_SERVER_MSG_ROUTE_CLASS(LoginLoad);
// 澶у巺缃戝叧 绠＄悊鍣ㄧ殑璺敱鍣?
DEF_SERVER_MSG_ROUTE_CLASS(LobbyGate);
// 澶у巺涓績 绠＄悊鍣ㄧ殑璺敱鍣?
DEF_SERVER_MSG_ROUTE_CLASS(LobbyCenter);
// 瀹跺洯鏈嶅姟鍣?
DEF_SERVER_MSG_ROUTE_CLASS(HomeLogic);
// 鍖归厤 绠＄悊鍣ㄧ殑璺敱鍣?
DEF_SERVER_MSG_ROUTE_CLASS(MatchLogic);


/*
// 绀轰緥浠ｇ爜
// 鐩爣瀵硅薄 鍩虹璺敱鎺ュ彛绫?
class MyExampleUser : public SDB_CLASS(SManager), public SDB_CLASS(SLauncher)
{
public:
    void Init() {
        // 璺敱瑙勫垯 璁剧疆鐩爣鏈嶅姟鍣ㄧ殑UID
        SetSManagerSUID(100);
        // 璺敱瑙勫垯 璁剧疆鐩爣鏈嶅姟鍣ㄧ殑UID
        SetSLauncherSUID(100);

    }

    //
    void SendMsg() {
        SendToSManager(0, 0, 0);
    }
};
*/



