#pragma once

#include "xBase/xDefine.h"
#include "ShareDef.h"

#ifdef    _WIN32
#define INVALID_SM_HANDLE     ((void*)0)
#else
#define INVALID_SM_HANDLE     -1
#endif

namespace    ShareMemAPI {

    /*鍒涘缓ShareMem 鍐呭瓨鍖?
     *
     *    key   鍒涘缓ShareMem 鐨勫叧閿€?
     *
     *  Size  鍒涘缓澶у皬
     *
     *    杩斿洖 瀵瑰簲ShareMem淇濇寔鍊?
     */
    SMHandle        CreateShareMem(SM_KEY key, uint32 Size);
    /*鎵撳紑ShareMem 鍐呭瓨鍖?
     *
     * key   鎵撳紑ShareMem 鐨勫叧閿€?
     *
     * Size  鎵撳紑澶у皬
     *
     * 杩斿洖  瀵瑰簲ShareMem 淇濇寔鍊?
     */
    SMHandle        OpenShareMem(SM_KEY key, uint32 Size);

    /*
     * 娓呴櫎鎺夊叡浜唴瀛?
     */
    void            Clean(SM_KEY);

    /*鏄犲皠ShareMem 鍐呭瓨鍖?
     *
     *    handle 鏄犲皠ShareMem 鐨勪繚鎸佸€?
     *
     *  杩斿洖 ShareMem 鐨勬暟鎹寚閽?
     */
    char* MapShareMem(SMHandle handle);

    /*鍏抽棴鏄犲皠 ShareMem 鍐呭瓨鍖?
     *
     *    MemoryPtr            ShareMem 鐨勬暟鎹寚閽?
     *
     *
     */
    void            UnMapShareMem(char* MemoryPtr);

    /*    鍏抽棴ShareMem
     *     handle  闇€瑕佸叧闂殑ShareMem 淇濇寔鍊?
     */
    void            CloseShareMem(SMHandle handle);

}



