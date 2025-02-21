#pragma once

#include "xDefine.h"


#ifdef _WIN32

inline std::string GGbkToUtf8(const wchar_t *wstr) {
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    char* str = new char[len + 1];
    memset(str, 0, len + 1);
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str, len, NULL, NULL);
    std::string strTemp = str;
    if (str) delete[] str;
    return strTemp;
}

inline std::wstring GUtf8ToGbk(const char *srcStr) {
    int len = MultiByteToWideChar(CP_UTF8, 0, srcStr, -1, NULL, 0);
    wchar_t* wszGBK = new wchar_t[len + 1];
    memset(wszGBK, 0, len * 2 + 2);
    MultiByteToWideChar(CP_UTF8, 0, srcStr, -1, wszGBK, len);
    std::wstring strRet(wszGBK);
    if(wszGBK)delete[]wszGBK; wszGBK = nullptr;
    return strRet;
}

#else

#include <iconv.h>

inline int GGbkToUtf8Ex(char *str_str, size_t src_len, char *dst_str, size_t dst_len) {
    iconv_t cd;
    char **pin = &str_str;
    char **pout = &dst_str;

    cd = iconv_open("utf8", "gbk");
    if (cd == 0)
        return -1;
    memset(dst_str, 0, dst_len);
    iconv(cd, pin, &src_len, pout, &dst_len);
    //if (iconv(cd, pin, &src_len, pout, &dst_len) == -1)
    //    return -1;
    iconv_close(cd);
    *pout = '\0';

    return 0;
}

inline string GGbkToUtf8(const std::string &strStr) {
    char tmpData[65535] = { 0 };
    if (0 == GGbkToUtf8Ex((char *)strStr.c_str(), strStr.size(), tmpData, 65535)) {
        return tmpData;
    }
    return "";
}


inline int GUtf8ToGbkEx(char *srcStr, size_t srcLen, char *dstStr, size_t dstLen) {
    iconv_t cd;
    char **pin = &srcStr;
    char **pout = &dstStr;

    cd = iconv_open("gbk", "utf8");
    if (cd == 0)
        return -1;
    memset(dstStr, 0, dstLen);
    iconv(cd, pin, &srcLen, pout, &dstLen);
    //if (iconv(cd, pin, &srcLen, pout, &dstLen) == -1)
    //    return -1;
    iconv_close(cd);
    *pout = '\0';

    return 0;
}

inline string GUtf8ToGbk(const std::string &strStr) {
    char tmpData[65535] = { 0 };
    if (0 == GUtf8ToGbkEx((char *)strStr.c_str(), strStr.size(), tmpData, 65535)) {
        return tmpData;
    }
    return "";
}

#endif

inline bool GIsStrUtf8(const char* str) {
    unsigned int nBytes = 0;
    unsigned char chr = *str;
    bool bAllAscii = true;
    for (unsigned int i = 0; str[i] != '\0'; ++i) {
        chr = *(str + i);
        if (nBytes == 0 && (chr & 0x80) != 0) {
            bAllAscii = false;
        }
        if (nBytes == 0) {
            if (chr >= 0x80) {
                if (chr >= 0xFC && chr <= 0xFD) {
                    nBytes = 6;
                }
                else if (chr >= 0xF8) {
                    nBytes = 5;
                }
                else if (chr >= 0xF0) {
                    nBytes = 4;
                }
                else if (chr >= 0xE0) {
                    nBytes = 3;
                }
                else if (chr >= 0xC0) {
                    nBytes = 2;
                }
                else {
                    return false;
                }
                nBytes--;
            }
        }
        else {
            if ((chr & 0xC0) != 0x80) {
                return false;
            }
            nBytes--;
        }
    }
    if (nBytes != 0) {
        return false;
    }
    if (bAllAscii) { 
        return true;
    }
    return true;
}

inline bool GIsStrGbk(const char* str) {
    unsigned int nBytes = 0;
    unsigned char chr = *str;
    bool bAllAscii = true; 
    for (unsigned int i = 0; str[i] != '\0'; ++i) {
        chr = *(str + i);
        if ((chr & 0x80) != 0 && nBytes == 0) {
            bAllAscii = false;
        }
        if (nBytes == 0) {
            if (chr >= 0x80) {
                if (chr >= 0x81 && chr <= 0xFE) {
                    nBytes = +2;
                }
                else {
                    return false;
                }
                nBytes--;
            }
        }
        else {
            if (chr < 0x40 || chr>0xFE) {
                return false;
            }
            nBytes--;
        }//else end
    }
    if (nBytes != 0) {   
        return false;
    }
    if (bAllAscii) { 
        return true;
    }
    return true;
}

// 
// int main(void) {
//     char *src_str = "闂佹垝绫嶉妶鍡椻偓妤冪磽閸パ囧弰闁告﹩鍘藉鍕煥閸愩劌杈呴梺?;
//     cout << "origin string: " << src_str << endl;
// 
// #ifdef _WIN32
//     // windows default is gbk
//     string dst_str = GGbkToUtf8(src_str);
//     cout << "gbk to utf8: " << dst_str << endl;
// 
//     string str_utf8 = GUtf8ToGbk(dst_str.c_str());
//     cout << "utf8 to gbk: " << str_utf8 << endl;
// #else
//     // unix default is utf8
//     char dst_gbk[1024] = { 0 };
//     GUtf8ToGbk(src_str, strlen(src_str), dst_gbk, sizeof(dst_gbk));
//     cout << "utf8 to gbk: " << dst_gbk << endl;
// 
//     char dst_utf8[1024] = { 0 };
//     GGbkToUtf8(dst_gbk, strlen(dst_gbk), dst_utf8, sizeof(dst_utf8));
//     cout << "gbk to utf8: " << dst_utf8 << endl;
// #endif
// 
//     return 0;
// }
// 







