#pragma once

#include "xBase/xObjPool.h"
#include "xNet/xNetProcessor.h"
#include "xDatabase/RecordData.h"

#define SERVER_HEARTBEAT 20


// 澶勭悊娑堟伅鍙戦€佷氦鎹㈢鐞嗗櫒
// 閬垮厤娑堟伅鎷疯礉鍜屽绾跨▼鍙戦€侀棶棰?
// 涓昏瑙ｅ喅 閫昏緫绾跨▼ 鍒扮綉缁滅嚎绋嬪彂閫佹秷鎭棶棰?
#define SWAP_POOL_NORMAL_COUNT 5
class Logic2NetSendMsgSwap
{
public:
    Logic2NetSendMsgSwap();
    ~Logic2NetSendMsgSwap();

public:
    typedef std::list<RecordCacheData*> RecordCacheDataList;

public:
    // 閫昏緫灞傝幏鍙栧皢瑕佸彂閫佺殑鍐呭瓨缂撳瓨瀵硅薄 
    RecordCacheData *GetRecordCacheData();
    // 鍒犻櫎鎺夌紦瀛樿妭鐐?
    void DelRecordCacheData(RecordCacheData *);
    // 褰撴暟鎹缃ソ涔嬪悗鏀惧叆缃戠粶灞傜殑鍙戦€佸垪琛?
    void AddSend(RecordCacheData *pData);

public:
    // 缃戠粶灞傝皟鐢ㄥ彂閫佸緟鍙戦€佺殑鍒楄〃 
    void RunSendMsg(xNetProcessor *pNetProcess);

private:
    // 鐢ㄤ簬鐩戣缂撳瓨姹犵殑瀹氭椂鍣ㄥ彉閲?
    time_t _pre_show_pool_size = 0;
    // 鐢ㄤ簬涓庨€昏緫灞傚仛娑堟伅鍙戦€佺殑浜ゆ崲鍐呭瓨
    // 2000 * 2000 = 4M
    xObjPool<RecordCacheData> m_poolSwapMem;

private:
    // 闇€瑕佸彂閫佺殑鍒楄〃 
    std::mutex m_lockMsgMap;
    // 浜ゆ崲鏁版嵁鍐呭瓨鐨勫垪琛?
    RecordCacheDataList mRecordCacheDataList;
};


// 鏈嶅姟鍣ㄩ摼鎺?
class ServerClient
{
    friend class ZoneServer;
public:
    ServerClient();
    ~ServerClient();

public:
    bool sendCmd(const void *cmd, unsigned short len);
    bool getCmd(unsigned char *&cmd, unsigned short &len);
    bool popCmd();
    void put(unsigned char *cmd, unsigned short len);
    inline bool get(unsigned char *&cmd, unsigned short &len);
    void pop();
    xNetProcessor *getNp();
    const char* getName();
    bool check();
    void reset();

public:
    xNetProcessor *pNetProcess = nullptr;

private:
    int _heartbeat = 0;
};



