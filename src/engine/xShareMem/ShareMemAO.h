#pragma once

#include "xBase/xDefine.h"
#include "ShareDef.h"


#pragma pack(push, 1)
struct SMHead
{
    SM_KEY            m_Key;
    unsigned long    m_Size;
    uint32            m_HeadVer; //鏈€鍚庡瓨鐩樼増鏈?
    SMHead() {
        m_Key = 0;
        m_Size = 0;
        m_HeadVer = 0;
    }
};
#pragma pack(pop)

#define        SHMEM_LOG_PATH    "./Log/ShareMemory.log"
/*
 *        鍏变韩鍐呭瓨璁块棶瀵硅薄
 *        ShareMemory    Access    Object
 */
class ShareMemAO
{
public:
    ShareMemAO() {
        m_pDataPtr = 0;
        m_hold = 0;
        m_Size = 0;
        m_pHeader = 0;
    }
    ~ShareMemAO() {};
    /*
     *    鍒涘缓ShareMem 璁块棶瀵硅薄(鏂板垱寤?
     *
     *  SM_KEY    key        璁块棶閿€?
     *
     *    uint        Size    璁块棶鏁版嵁鍖哄瓧鑺備釜鏁?
     *
     */
    bool    Create(SM_KEY key, uint32 Size);
    /*
     *    閿€姣佸璞?
     */
    void    Destory();

    /*
     *        闄勭潃ShareMem 璁块棶瀵硅薄(涓嶆槸鏂板垱寤?
     *        SM_KEY    key        璁块棶閿€?
     *
     *        uint        Size    璁块棶鏁版嵁鍖哄瓧鑺備釜鏁?
     *
     */
    bool    Attach(SM_KEY, uint32 Size);

    void    Clean(SM_KEY);
    /*
     *        鍙栨秷闄勭潃(涓嶉攢姣?
     */
    bool    Detach();

    /*
     *     鑾峰緱鏁版嵁鍖烘寚閽?
     */
    char* GetDataPtr() {
        return m_pDataPtr;
    }

    /*
     *    鑾峰緱 澶у皬涓簍Size 鐨勭tIndex 涓猻mu鐨勬暟鎹?
     */
    char* GetTypePtr(uint32 tSize, uint32 tIndex) {
        assert(tSize > 0);
        assert((int32)(tSize * tIndex) < m_Size);
        if (tSize <= 0 || (int32)tIndex >= m_Size) {
            return nullptr;
        }
        return m_pDataPtr + tSize * tIndex;
    }
    /*
     *    鑾峰緱鏁版嵁鍖烘€诲ぇ灏?
     */
    uint32    GetSize() {
        return m_Size;
    }

    bool    DumpToFile(char* FilePath);
    bool    MergeFromFile(char* FilePath);

    uint32    GetHeadVer();
    void    SetHeadVer(uint32 ver);

public:
    //鍛戒护
//     int32                    m_CmdArg;

private:

    //ShareMemory    鍐呭瓨澶у皬
    int32                m_Size = 0;
    // ShareMemory  鏁版嵁鎸囬拡
    char* m_pDataPtr = nullptr;
    // ShareMemory    鍐呭瓨澶存寚閽?
    char* m_pHeader = nullptr;
    // ShareMemory    鍙ユ焺    
    SMHandle            m_hold = 0;
};



