#pragma once

// 榛樿璺敱灞傜骇
#define ROUTE_NODE_LEVEL_NORMAL 3


// 璐熻浇瀹氫箟
#define SERVER_LOAD_WARNING 2000
#define SERVER_LOAD_FULL 4000

// 鍗忚娉ㄥ唽鐨勬墽琛岀嚎绋嬬被鍨?
enum CMD_RUN_THREAD_TYPE
{
    // 缃戠粶绾跨▼鎵ц
    CMD_RUN_THREAD_NET = 0,
    // 閫昏緫绾跨▼鎵ц
    CMD_RUN_THREAD_LOGIC,

    CMD_RUN_THREAD_MAX,
};

// 鏈嶅姟鍣ㄧ被鍨婭D瀹氫箟 
// 娉ㄦ剰 闇€瑕佸拰 t_servername 涓殑瀹氫箟涓€涓€瀵瑰簲 
enum SERVER_ROUTE_SID_DEF
{
    /* *******************************************
     *
     *    SManager 鍜?SLauncher 鏈嶅姟鍣ㄤ负绠＄悊鏈嶅姟鍣ㄥ苟闈炴父鎴忛€昏緫鏈嶅姟鍣?
     *
     */

    SERVER_SID_LOGIN_GATE = 1,    // 鐧诲綍鏈嶅姟鍣?
    SERVER_SID_DATE_RECORD = 2,    // 鏁版嵁鏈嶅姟鍣?
    // 澶у巺鏈嶅姟鍣?
    SERVER_SID_LOBBY_GATE = 3,    // 澶у巺缃戝叧鏈嶅姟鍣?
    SERVER_SID_LOBBY_CENTER = 103,  // 澶у巺涓績鏈嶅姟鍣?
    // 瀹跺洯鏈嶅姟鍣?
    SERVER_SID_HOME_LOGIC = 4,    // 瀹跺洯閫昏緫鏈嶅姟鍣?
    SERVER_SID_HOME_CENTER = 104,  // 瀹跺洯涓績鏈嶅姟鍣?

    SERVER_SID_BATTLRREPORT_LOGIC = 5,    // 鎴樻姤鏈嶅姟鍣?

    // 鍖归厤鏈嶅姟鍣?
    SERVER_SID_MATCH = 10,   // 鍖归厤鏈嶅姟鍣?

    // 骞冲彴濂藉弸鏈嶅姟鍣ㄧ粍
    SERVER_SID_FRIEND_LOGIC = 11,   // 濂藉弸鏈嶅姟鍣?閫昏緫鏈嶅姟鍣?
    SERVER_SID_FRIEND_CENTER = 111,  // 濂藉弸鏈嶅姟鍣?涓績鏈嶅姟鍣?
    SERVER_SID_FRIEND_ROUTE = 12,   // 濂藉弸鏈嶅姟鍣?鍏ㄥ眬璺敱鏈嶅姟鍣?

    // 骞冲彴鏀粯鏈嶅姟鍣ㄧ粍
    SERVER_SID_MARKET_LOGIC = 15,   // 鏀粯鏈嶅姟鍣?閫昏緫鏈嶅姟鍣?
    SERVER_SID_MARKET_CENTER = 115,  // 鏀粯鏈嶅姟鍣?涓績鏈嶅姟鍣?瀵瑰簲涓€涓暟鎹腑蹇冩湇鍔＄殑鐜╁涓氬姟
    SERVER_SID_MARKET_ROUTE = 16,   // 鏀粯鏈嶅姟鍣?鍏ㄥ眬璺敱鏈嶅姟鍣?

    SERVER_SID_MAIL_SERVER = 18, // 閭欢鏈嶅姟鍣?
    SERVER_SID_GM_SERVER = 19, // GM 鏈嶅姟鍣?

    SERVER_SID_GAME_LOG         = 20,   // 娓告垙鏃ュ織鏈嶅姟鍣?
};





