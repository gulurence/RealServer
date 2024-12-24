#pragma once

#include "xBase/xDefine.h"
#include "xBase/xSingleton.h"
#include "xShareMem/ShareMemAO.h"
#include "xServerMgrDataDef.h"


/*
 鍚姩鐨勬父鎴忚繘绋嬩娇鐢ㄥ叡浜唴瀛樿繘琛屽弬鏁颁紶閫?
*/

// 杩欐槸鍗曞彴鐗╃悊鏈虹殑鏈嶅姟鍣ㄩ厤缃暟閲?
// 杩愯鐨勬湇鍔″櫒鑺傜偣鏁伴噺 
#define DEFAULT_NODE_COUNT   30
// 姣忎釜鑺傜偣鍙互瀛樻柟鐨勯厤缃暟鎹殑鏈€澶уぇ灏?
#define DEFAULT_NODE_SIZE    4096
// key for share mem
#define SHARE_MEM_KEY_ID    0x0167CAF5

// 鎵╁睍绌洪棿鎻愪緵缁欏叾浠栨湇鍔″櫒鐢ㄤ綔缂撳瓨绌洪棿

// sguid - ptr
typedef std::map<uint32, uint32> ShareIndexMap;

// 杩欓噷璐熻矗鍏变韩鍐呭瓨鐨?缁存姢
class GameServerRunShareMgr : public xSingleton<GameServerRunShareMgr>
{
public:
    GameServerRunShareMgr(){}
    ~GameServerRunShareMgr(){}

private:
    bool m_bIsInit = false;
    // sguid 瀵瑰簲鐨勫叡浜唴瀛樻寚閽?
    ShareIndexMap m_mapShareIndex;

private:
    ShareMemAO m_shareMenMgr;

public:
    bool Init();
    bool Attach(SM_KEY, uint32 Size);
    void Clean(SM_KEY);
    void Destroy();

public:
    int  GetEmptyNode();
    int  GetIDNodeIndex(uint32);
    int  GetIDNodeSGUID(int );
    uint64 GetSGUIDProcessID(uint32 sguid);
    void RemoveNode(uint32 );
    int  UpdateNodeData(uint32 nodeKey, const char *sname, const char *nodeData, const int &nodeLen);

    const char *GetNodePtr(int sguid);
};



