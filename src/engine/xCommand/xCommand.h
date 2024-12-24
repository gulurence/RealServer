#pragma once

#include "../xBase/xTools.h"
#include "../xCommand/xMsgToolMac.h"
#include "xServerRouteDefine.h"

#include <iostream>


#define MAX_CMD                 255
#define MSG_DISPATCHER_CMD      1
#define SERVICES_CMD            2
#define MANAGE_CMD              3
#define LOG_CMD                 4
#define SERVER_LOAD_CMD         5



#pragma pack(1)

// net packet head
struct xCommand
{
    uint8 cmd = 0;
    uint8 param = 0;

    xCommand(uint8 cmd_ = 0, uint8 param_ = 0) :cmd(cmd_), param(param_) {
    }
    bool is(uint8 cmd_) {
        return cmd == cmd_;
    }
    bool operator==(const xCommand &in) const {
        return (cmd == in.cmd) && (param == in.param);
    }
    bool operator<(const xCommand &in) const {
        if (cmd == in.cmd) return param > in.param;
        return (cmd > in.cmd);
    }
};



/* *****************************************
 *
 *  SGUID_UINT32 濞戞挸鎼弫顔界▔閳ь剟寮介崶顏嶅敹濞戞挴鍋撳☉鎿冧簼濠€鍥礉閳ヨ櫕鐝ら弶鈺傜〒閳诲ジ鎯冮崟顒傚灱閻犲洤鏀╠闁挎稑濂旂粭鏍偨閵娿倗鑹鹃柡鍫濈Т婵喖宕抽妸鈺傦紵闁汇劌瀚粔鐑藉箒椤栨繄鐔呴柣銏犲船閻ｇ偓鎷? *
 *
 *  <sid>濞戞挾鍎ゅ﹢鍥礉閳ヨ櫕鐝ょ紒澶婄Ф鐞氱帵d 闁告帒妫旂粭鍛存椤厾鐟忕紒澶婄Ф鐞氼偊宕?闁?鐎殿喒鍋撳┑顔碱儍閳ь剚鍨埀顒佸姇瀵剛鎲?t_servername 閻庤鐭粻鐔煎Υ? *  1.濞戞挸绉撮悽顐も偓娑欏姌婵☆參鎮欓崷顓熺暠濞戞搩鍘肩缓楣冩嚍閸屾粌浠弶鈺傜〒閳?婵絾鏌ㄩ々?DataRecord/LoginGate  闁靛棙浠抜d闊洤鎳橀妴蹇旂▔?闁? *  2.閻㈩垽绠戦悺娆撴嚍閸屾粌浠柣銊ュ閼垫垼绠涢崘顓炍濋柣鎰攰缁绘绮?婵絾鏌ㄩ々? LobbyCenter/HomeCenter  闁靛棙浠抜d闊洤鎳橀妴蹇旂▔?闁? *  3.缂傚洢鍊楃划宥夊嫉瀹ュ懎顫ら柛锝冨妿濞堟垹鈧稒鍔樻俊顓㈡倷绾懐绠荤紒?婵絾鏌ㄩ々?LobbyGate/HomeLogic  闁靛棙浠抜d闊洤鎳橀妴蹇旂?鐎殿喒鍋撳┑顔碱儍閳? *
 *  <gid>濞戞挾鐨d濞戞挸顑囧▓鎴犵礃閵堝洨鐭嬮柡鍫濈Т婵喖宕抽埡顧柨娑樼灱濞村宕ョ€圭杯d濞戞挻姊诲ù澶愬触鐎圭剾d濞寸媴缍€閵嗗啯绋夐埀顒佺▔椤忓棗鍙冪紓浣稿濠€鍥礉閳ヨ櫕鐝?
 *
 *  <nid>濞戞挾顕癷d濞戞挸顑囧▓鎴犫偓娑欏姌婵☆參鎮欑粵鐧查柨娑樼灱濞村宕ョ€圭杯d濞戞挻姊诲ù澶愬触鐎圭剾d妤犵偞婀圭粭鏉梚d濞戞挸绉撮幃鎾寸閿濆牄鈧啴鎯勭粙鎸庡€辩紓鍥ュ€楃划宥嗙▔椤撶姵鐣遍悗娑欏姌婵☆參鎮欑憴鍕疀闁告柡鈧櫕鐝?
 *
 */
// 闁哄牃鍋撳鍫嗗懏鐣遍柕鍡樺姇閻℃瑩鎳為崒婊冧化闁哄牆绉存慨鐔煎闯閵婏絺鍋?濞寸姰鍎卞?闁靛棙鍔掔粭澶屾暜閿曗偓閻℃瑩鎳為崒婊冧化闁汇劌瀚懙鎴ｇ疀閸愵厼螡闁绘劘顫夊﹢鍥礉閳ヨ櫕鐝ら柕?sid
#define CENTER_SERVER_CHILD_MAX_SID 80
// 閻㈩垽绠戦悺娆撴嚍閸屾粌浠柣銊ュ閼垫垼绠涢崘顓炍濋柣鎰暉id闊洤鎳橀妴蹇斿緞瑜岀花顒佺▔鐎ｎ喗妗ㄩ悗瑙勭煯缁?
#define CENTER_SERVER_BASE_SID 100
struct SGUID_UINT32
{
private:

    struct _Data
    {
        uint8 sid;      // 1~255
        uint8 nid;      // 1~255
        uint16 gid;     // 1~65535
    };

    union
    {
        uint32 u32Data = 0;
        _Data _data;
    }ID;

public:
    SGUID_UINT32() {
        ID.u32Data = 0;
    }
    SGUID_UINT32(uint32 sguid) {
        ID.u32Data = sguid;
    }
    SGUID_UINT32(const uint8 &_sid, uint16 _gid, uint8 _nid) {
        ID._data.sid = _sid;
        ID._data.gid = _gid;
        ID._data.nid = _nid;
    }
    SGUID_UINT32 &operator=(const uint32 &_data) {
        ID.u32Data = _data;
        return *this;
    }
    SGUID_UINT32 &operator=(const SGUID_UINT32 &_data) {
        if (this == &_data) {
            return *this;
        }
        ID.u32Data = _data.getSGUID();
        return *this;
    }
    bool operator==(const uint32 &_data) const {
        return ID.u32Data == _data;
    }

public:
    void setSGUID(uint32 sguid) {
        ID.u32Data = sguid;
    }
    void setSid(uint8 sid) {
        ID._data.sid = sid;
    }
    void setGid(uint16 _id) {
        ID._data.gid = _id;
    }
    void setNid(uint8 _nid) {
        ID._data.nid = _nid;
    }

public:
    // 闁哄嫷鍨伴幆渚€寮伴娑欑疀闁告柡鈧櫕鐝ょ紒顐ヮ嚙閻庣兘姊块崱娆忓弮濞戞搩鍙冨Λ鍧楁嚍閸屾粌浠?
    bool isSCenter() const {
        return ID._data.sid > CENTER_SERVER_BASE_SID || getNodeID() == 0;
    }
    uint32 getSGUID() const {
        return ID.u32Data;
    }
    // 闁哄牆绉存慨鐔煎闯閵娧嗩潶闁?闁糕晞娅ｉ、鍖
    uint32 getServerID() const {
        return ID._data.sid;
    }
    // 闂傚棗妫涢崗顢疍
    uint32 getGroupID() const {
        return (uint16)ID._data.gid;
    }
    // 闂傚棗妫涢崗銏ゆ嚍閸屾粌浠疘D
    uint32 getNodeID() const {
        return (uint8)ID._data.nid;
    }

public:
    // 閺夌儐鍓氬畷鏌ュ礆?SGUID_UINT32
    static SGUID_UINT32 ToSGUID(const int &guid) {
        SGUID_UINT32 stSGUID(guid);
        return stSGUID;
    }
    static uint32 GetCenterSGUID(uint32 sguid) {
        SGUID_UINT32 stSGUID(sguid);
        stSGUID.setSid(stSGUID.getServerID()>= CENTER_SERVER_BASE_SID?(stSGUID.getServerID()):(stSGUID.getServerID() + CENTER_SERVER_BASE_SID));
        stSGUID.setNid(0);
        return stSGUID.getSGUID();
    }
};

// save route info
struct RouteInfo
{
    // 闁告瑯鍨禍鎺旀崉椤栨粍鏆犻柣銊ュ閻壆鐥?婵絽绻楃换鍐╃▔閳ь剚绋夐鍛疀闁告柡鈧櫕鐝ら柛鎴濈箰閻? 闂侇剙鐏濋崢銈咁潰鐠囧弶鍎曢柣婊庡灟缁卞爼鏌?    // 濠㈠爢鍌滆壘0閻炴稏鍔庨妵姘跺矗椤栨瑤绨扮紓浣堝懐鏁惧ù鑲╁█閳?缂佹稑顦花?闁汇劌瀚鍌炲磹?闁哄牆绉存慨鐔煎闯閵娿儳娈哄☉鎾卞灩缁辨梻鎷犻妷褏绉圭紓浣圭矊鐎?
    char cNodeLevel = ROUTE_NODE_LEVEL_NORMAL;
    // 闁哄嫷鍨伴幆渚€宕ｉ幋锔瑰亾娴ｈ櫣鑸跺ù婊冪灱妤犲洭鎳為崒婊冧化
    char cIsToChiled = 0;
    // 闁哄嫷鍨伴幆渚€寮伴姘辩暛闁?濞撴碍绻冨畵渚€濡撮幁鐖刣 + cIsToChiled闁靛棙鍨跺鍨交濞戞粠鏀介弶鍫涘妼閸?
    char cIsBroadCast = 0;
    // 濞戞挸锕ｇ粩瀵哥棯瑜忓▓?SGUID 闁活潿鍔嬬花顒勭嵁閹稿孩灏￠柟鐑樺浮濞?濞戞挸绉撮崯鈧鐐村閹歌京绱掑▎搴ｇ憪缂佺嫏鍕疀闁告柡鈧櫕鐝?
    SGUID_UINT32 stPreSGUID[ROUTE_NODE_LEVEL_NORMAL];
    // 婵犙勨偓鐩橴ID
    SGUID_UINT32 stSrcSGUID;
    // 闁烩晩鍠楅悥顤橤UID
    SGUID_UINT32 stTargetSGUID;

    // 闁兼儳鍢茶ぐ鍥ㄧ▔閳ь剚绋夐鍡忔晞闁煎搫鍊婚崑?
    bool InPreNode(const uint32 &sguid) {
        for (int i = 0; i < ROUTE_NODE_LEVEL_NORMAL; ++i) {
            if (sguid == stPreSGUID[i].getSGUID()) {
                return true;
            }
        }

        return false;
    }
    bool AddPreNode(const uint32 &sguid) {
        if (InPreNode(sguid)) {
            return true;
        }
        for (int i = 0; i < ROUTE_NODE_LEVEL_NORMAL; ++i) {
            if (0 == stPreSGUID[i].getSGUID()) {
                stPreSGUID[i].setSGUID(sguid);
                return true;
            }
        }

        return false;
    }

    RouteInfo() {
    }
};

// route msg
struct xServerDispatcherMsg
{
    xCommand cmdID;
    // 闁荤偛妫涢弫杈ㄧ┍閳╁啩绱?
    RouteInfo stRouteInfo;
    // 閺夆晜鐟╅崳鐑藉及閻も暜otoBuff String 闁轰胶澧楀畵?
    uint32 msg_len = 0;
    char msg_data[0];// proto -闁?tostring

    void Init(uint8 cmd, uint8 param) {
        cmdID.cmd = cmd;
        cmdID.param = param;
    }
    void SetProtoData(char *buff, uint32 l) {
        msg_len = l;
        memcpy(msg_data, buff, msg_len);
    }
    uint32 GetTotalLen() {
        return msg_len + sizeof(xServerDispatcherMsg);
    }
    char *GetBuffer() {
        return (char*)this;
    }
    char *GetMsgBuffer() {
        return msg_data;
    }
};

// packet packing tool
struct xCommandSendBuffer
{
    xCommand cmdID;
    // data len
    uint32 len = 0;
    // proto -> tostring
    char data[0];

    void Init(uint8 cmd, uint8 param) {
        cmdID.cmd = cmd;
        cmdID.param = param;
    }
    void SetProtoData(char *buff, uint32 l) {
        len = l;
        memcpy(data, buff, len);
    }
    uint32 GetTotalLen() {
        return len + sizeof(xCommandSendBuffer);
    }
    char *GetBuffer() {
        return (char*)this;
    }
    char *GetProtoBuffer() {
        return data;
    }
};

#pragma pack()








