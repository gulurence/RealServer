/*****************************************************************
            Copyright (c) 2009, XXXXXX鐠侊紕鐣婚張铏诡潠閹垛偓閼测€插敜閺堝妾洪崗顒€寰?
                    All rights reserved

    閸掓稑缂撻弮銉︽埂閿?2009楠?2閺?閺?17閺?4閸?
    閺傚洣娆㈤崥宥囆為敍?SingletonTemplate.h
    鐠?   閺勫函绱?閸楁洑绶ュΟ鈥崇础閻ㄥ嫭纭鹃崹瀣杽閻滆埇鈧倹婀伴崡鏇炲帗閹绘劒绶垫稉銈勯嚋濞夋稑鐎烽崡鏇氱伐缁紮绱?

                閸掑棗鍩嗙€圭偟骞囨禍鍡曡⒈娑擃亣顕㈡稊澶夌瑐閻ㄥ嫬宕熸笟瀣剁礉 閸忔湹鑵戦敍?

                1. 缁眴iaSingleInstance<T>鐠虹兗閺勭棤s a閻ㄥ嫬鍙х化浼欑礉鎼存梻鏁ゅ銈喣佸蹇撳灟鐠囧瓨妲慣缁粯妲搁崣顖滄埛閹佃法娈戦敍灞惧娴狀櫄缁绻€闂団偓濠娐ゅ喕婵″倷绗呴弶鈥叉閿?
                1.1 姒涙顓婚弸鍕偓鐘插毐閺佹澘鎷伴弸鎰€崙鑺ユ殶閺勭棷ublic閹存潤rotected閻ㄥ嫯顔栭梻顔鹃獓閸?
                1.2 閺嬫劖鐎崙鑺ユ殶韫囧懘娓堕弰鐥竔rtual閻?

                2. 缁眴haSingleInstance<T>鐠虹兗閺勭棢as a閻ㄥ嫬鍙х化浼欑礉閸楀啿鍙ч懕鏂垮彠缁紮绱濇惔鏃傛暏濮濄倖膩瀵繐鍨拠瀛樻T閺勭枌haSingleInstance<T>闁?
                閻ㄥ嫪绔存稉顏堟饯閹焦鍨氶崨妯哄綁闁插繒琚崹瀣剁礉閹碘偓娴狀櫄缁绻€闂団偓濠娐ゅ喕婵″倷绗呴弶鈥叉閿?
                2.1 姒涙顓婚弸鍕偓鐘插毐閺佹澘鎷伴弸鎰€崙鑺ユ殶閺勭棷ublic閻ㄥ嫯顔栭梻顔鹃獓閸?

                瀵ら缚顔呴敍?. 娴兼ê鍘涢懓鍐娴ｈ法鏁haSingleInstance<T>
                2. typedef 鐏忓棔濞囨禒锝囩垳閺囨潙鍙块崣顖濐嚢閹嶇礉
                婵″偊绱皌ypedef ChaSingleInstance<T> TSingleInstance;

    瑜版挸澧犻悧鍫熸拱閿? 1.00
    娴?   閼板拑绱? 
    濮?   鏉╁府绱? 閸掓稑缂? 

    瑜版挸澧犻悧鍫熸拱閿? 1.01
    娴?   閼板拑绱? 
    濮?   鏉╁府绱? 濞ｈ濮炴径姘卞殠缁嬪宕熺€圭偘绶ラ弨顖涘瘮, 闂堟瑦鈧椒绶风挧鏍€庢惔蹇撳灥婵瀵查弨顖涘瘮

*********************************************************************/
#pragma once


template <typename T>
class CiaSingleton : public T
{
public:
    typedef T SingletonType;

public:
    // 閼惧嘲褰囬崡鏇氱伐
    static SingletonType * singleton();
    // 闁插﹥鏂侀崡鏇氱伐
    static void release();

private:
    // 閸楁洑绶ョ€圭偘缍嬮幐鍥嫛
    static SingletonType *sm_pSingleInstance;

private:
    CiaSingleton(const CiaSingleton &) = delete;
    const CiaSingleton & operator=(const CiaSingleton &) = delete;

    // 濮濄倗琚稉宥呭讲缂佈勫閿涘奔绗夐崣顖氱杽娓氬瀵?
    CiaSingleton() {};
    ~CiaSingleton() override {};

private:
    struct Init
    {
        Init() {
            CiaSingleton<T>::singleton();
        }
    };

    struct destroy
    {
        ~destroy() {
            if (nullptr == sm_pSingleInstance) {
                return;
            }

            delete sm_pSingleInstance;
            sm_pSingleInstance = nullptr;
        }
    };

private:
    static Init sm_Init;
};


template <typename T>
class ChaSingleton final
{
public:
    typedef T SingleInstanceType;

    // 閼惧嘲褰囬崡鏇氱伐
    static SingleInstanceType * singleton();
    // 闁插﹥鏂侀崡鏇氱伐
    static void release();

private:
    // 閸楁洑绶ョ€圭偘缍嬮幐鍥嫛
    static SingleInstanceType *sm_pSingleInstance;

private:
    ChaSingleton(const ChaSingleton &) = delete;
    const ChaSingleton & operator=(const ChaSingleton &) = delete;

    // 濮濄倗琚稉宥呭讲缂佈勫閿涘奔绗夐崣顖氱杽娓氬瀵?
    ChaSingleton() = delete;
    ~ChaSingleton() = delete;

private:
    struct Init
    {
        Init() {
            ChaSingleton<T>::singleton();
        }
    };

    struct Destroy
    {
        ~Destroy() {
            if (nullptr == sm_pSingleInstance) {
                return;
            }

            delete sm_pSingleInstance;
            sm_pSingleInstance = nullptr;
        }
    };

private:
    static Init sm_Init;
};


// 缁眴iaSingleInstance閻ㄥ嫬鐣炬稊?
template <typename T>
typename CiaSingleton<T>::SingletonType * CiaSingleton<T>::sm_pSingleInstance = nullptr;

template <typename T>
typename CiaSingleton<T>::SingletonType * CiaSingleton<T>::singleton() {
    if (nullptr == sm_pSingleInstance) {
        sm_pSingleInstance = new CiaSingleton<T>();
        static destroy s_Des;
    }

    return sm_pSingleInstance;
}

template <typename T>
void CiaSingleton<T>::release() {
    if (nullptr != sm_pSingleInstance) {
        delete sm_pSingleInstance;
        sm_pSingleInstance = nullptr;
    }
}

template <typename T>
typename CiaSingleton<T>::Init CiaSingleton<T>::sm_Init;


// 缁眴haSingleInstance閻ㄥ嫬鐣炬稊?
template <typename T>
typename ChaSingleton<T>::SingleInstanceType * ChaSingleton<T>::sm_pSingleInstance = nullptr;

template <typename T>
typename ChaSingleton<T>::SingleInstanceType * ChaSingleton<T>::singleton() {
    if (nullptr == sm_pSingleInstance) {
        sm_pSingleInstance = new SingleInstanceType();
        static Destroy s_Des;
    }

    return sm_pSingleInstance;
}

template <typename T>
void ChaSingleton<T>::release() {
    if (nullptr != sm_pSingleInstance) {
        delete sm_pSingleInstance;
        sm_pSingleInstance = nullptr;
    }
}

template <typename T>
typename ChaSingleton<T>::Init ChaSingleton<T>::sm_Init;







