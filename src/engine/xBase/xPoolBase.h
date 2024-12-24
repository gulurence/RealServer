/********************************************************************
            Copyright (c) 2009, XXXXXX鐠侊紕鐣婚張铏诡潠閹垛偓閼测€插敜閺堝妾洪崗顒€寰?
                   All rights reserved

    閸掓稑缂撻弮銉︽埂閿?2009楠?2閺?0閺?10閺?4閸?
    閺傚洣娆㈤崥宥囆為敍?PoolBase.h
    鐠?   閺勫函绱?閸愬懎鐡ㄥЧ鐘插缁崵娈戦崘鍛摠濮圭姴鐔€缁鐤勯悳?

    瑜版挸澧犻悧鍫熸拱閿?1.00
    娴?   閼板拑绱?
    濮?   鏉╁府绱?閸掓稑缂?
    娴ｈ法鏁ょ拠瀛樻:   濮濄倗琚弰顖欑娑擃亝濞婄挒锛勮閿涘奔绗夐崣顖氱杽娓氬瀵查敍灞界暊鐏忎浇顥婃禍鍡樼潨閻ㄥ嫬鐤勯悳鏉挎嫲鐎电瓗閻ㄥ埖ew閸滃畳elete閺傝纭舵潻娑滎攽闁插秷娴?
                娴犺缍嶉幆铏潨閸栨牜娈戠猾璁崇閺冿附娣抽悽?缂佈勫)閼疯櫕顒濈猾浼欑礉鐏忓崬鍙块張澶嬬潨閻ㄥ嫬濮涢懗濮愨偓鍌涱劃缁粯妲告稉鈧稉顏吥侀弶璺ㄨ閿涘本婀佹稉澶夐嚋
                濡剝婢橀崣鍌涙殶閿涘苯鍨庨崚顐ｆЦ: T, BatchSize, Alloc閿涘苯鐣犳禒顒€濮涢懗钘夌暰娑斿妲搁敍?
                1. T
                   鐟曚焦鐫滈崠鏍畱鐎涙劗琚?
                2. BatchSize
                   濮ｅ繑顐奸悽瀹狀嚞濮圭姷娈戞径褍鐨敍灞藉祮瀵偓鏉堢喎顦跨亸鎴滈嚋T閸愬懎鐡ㄧ粚娲？
                3. Alloc
                   濮圭姴鍨庨柊宥呮珤, 閹稿绔寸€规氨娈戠憴鍕壐鐎圭偟骞囬敍宀€鏁ら幋铚傜瘍閸欘垵鍤滃鍗炵杽閻滄媽顫夐弽鐓庡閻ㄥ嫬鍨庨柊宥呮珤

               娴ｈ法鏁ゆ笟瀣摍:
                   class SubClass : public PoolBase<SubClass, 100>
                   {
                   ......
                   }

                   ...
                   // 娴犲孩鐫滄稉顓犳暤鐠囧嘲鍞寸€涙ê娼?
                   SubClass *pSubClass = new SubClass();
                   ...
                   // 鐏忓棗鍞寸€涙ê娼℃潻鏃囩箷缂佹瑦鐫?
                   delete pSubClass;


               ps: 濮濄倗琚稉宥嗘暜閹镐礁鐖㈤弫鎵矋閻ㄥ嫮鏁电拠鍑ょ礉婵?new T[12]閿涘苯鐣犵亸鍡楊嚤閼峰绱拠鎴濄亼鐠愩儯鈧倸鍙挎担鎾冲敶鐎圭顕憴浣规瀮濡楋絻鈧?
                   PoolBase 娑撳秵鏁幐浣割樋缁捐法鈻奸妴?
               閵嗏偓閵嗏偓
    瑜版挸澧犻悧鍫熸拱閿?1.01
    娴?   閼板拑绱?
    濮?   鏉╁府绱?婢х偛濮炴径姘卞殠缁嬪膩閺夎法琚玀TPoolBase, 娴ｈ法鏁ら弬瑙勭《閸氬oolBase閵?
*********************************************************************/
#pragma once

#include <vector>
#include "xSimpleAllocator.h"

#ifndef AllowPoolBase
template <
    typename T,             // 閸愬懎鐡ㄩ崚鍡涘帳閻ㄥ嫮琚崹?
    size_t BatchSize        // 濮ｅ繑顐煎鈧潏鐔奉樋鐏忔垳閲淭閸愬懎鐡ㄧ粚娲？
>
struct PoolBase
{
};

// 婢舵氨鍤庣粙?
template <typename T>
struct MTPoolBase
{
};

#else

template <
    typename T,                                               // 閸愬懎鐡ㄩ崚鍡涘帳閻ㄥ嫮琚崹?
    size_t BatchSize,                                         // 濮ｅ繑顐煎鈧潏鐔奉樋鐏忔垳閲淭閸愬懎鐡ㄧ粚娲？
    template <typename, size_t> class Alloc = SimpleAllocator // 閸掑棝鍘ら崳顭掔礉姒涙顓绘稉绡爄mpleAllocator
>
class PoolBase
{
    typedef Alloc<T, BatchSize> MyAlloc;
public:
    virtual ~PoolBase() {
    }

    // 闁插秷娴嘥閻?new 閸?delete
    void * operator new(size_t s) {
        return myAlloc.allocateBlock();
    }

    void operator delete(void *p) {
        myAlloc.releaseBlock((T *)p);
    }

private:
    // 娑撳秵鏁幐浣规殶缂佸嫮鏁电拠?
    void* operator new[](size_t s) {
        return nullptr;
    }

        void operator delete[](void* p) {
    };

private:
    // 闁藉牆顕甌缁鐎烽幍鈧張澶婄杽娓氬娈戦崡鏇氱濮圭姴鍨庨柊?
    static MyAlloc myAlloc;
};

template <typename T, size_t BatchSize, template <typename, size_t> class Alloc>
typename PoolBase<T, BatchSize, Alloc>::MyAlloc PoolBase<T, BatchSize, Alloc>::myAlloc;


template <
    typename T,                                               // 閸愬懎鐡ㄩ崚鍡涘帳閻ㄥ嫮琚崹?
    size_t BatchSize,                                         // 濮ｅ繑顐煎鈧潏鐔奉樋鐏忔垳閲淭閸愬懎鐡ㄧ粚娲？
    template <typename, size_t> class Alloc = MTSimpleAllocator // 閸掑棝鍘ら崳顭掔礉姒涙顓绘稉绡桾SimpleAllocator
>
class MTPoolBase
{
    typedef Alloc<T, BatchSize> MTMyAlloc;
public:
    virtual ~MTPoolBase() {
    }

    // 闁插秷娴嘥閻?new 閸?delete
    void* operator new(size_t s) {
        return myAlloc.allocateBlock();
    }

    void operator delete(void* p) {
        myAlloc.releaseBlock((T*)p);
    }

private:
    // 娑撳秵鏁幐浣规殶缂佸嫮鏁电拠?
    void* operator new[](size_t s) {
        return nullptr;
    }

        void operator delete[](void* p) {
    };

private:
    // 闁藉牆顕甌缁鐎烽幍鈧張澶婄杽娓氬娈戦崡鏇氱濮圭姴鍨庨柊?
    static MTMyAlloc myAlloc;
};

template <typename T, size_t BatchSize, template <typename, size_t> class Alloc>
typename MTPoolBase<T, BatchSize, Alloc>::MTMyAlloc MTPoolBase<T, BatchSize, Alloc>::myAlloc;

#endif // AllowPoolBase







