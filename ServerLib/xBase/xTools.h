#pragma once

#include "xDefine.h"
#include <map>
#include <vector>
#include <random>
#include <functional>
#ifndef _WINDOWS
#include <unistd.h>
#include <sys/socket.h>
#include "xLog.h"
#else
#include <winsock2.h>
#include <time.h>
#endif
#include <stdarg.h>


struct Tools
{
    public:
        Tools(){};

        static std::map<std::string, std::string> global;
        static bool initGlobal();

    private:
//        static std::uniform_int<> distribution(0, 99);
        static std::mt19937 _engine;
};

UInt32 stringTok(std::string s, std::string k, std::vector<std::string> &v);
UInt32 split(std::string str,std::string pattern,std::vector<std::string> &result);
template <typename T>
inline T* constructInPlace(T* p)
{
    return new(p) T;
}

/*template <typename T>
inline void* constructInPlace(T* p)
{
    new (static_cast<void *>(p)) T();
}
*/

#define SAFE_DELETE(p) do {delete p; p = NULL;} while(false)

#define SAFE_DELETE_VEC(p) do {delete[] p; p = NULL;} while(false)

inline void SAFE_CLOSE_SOCKET(int &fd)
{
    //XDBG("[Socket]close %d", fd);

    int ret = 0;

#ifndef _WINDOWS
    ret = shutdown(fd, SHUT_RDWR);
    ret = close(fd);
#else
    ret = closesocket(fd);
#endif
    if (0!=ret)
        XERR("[Socket]closesocket failed fd%d ret%d", fd, ret);

    fd = -1;
}

/*
inline void SAFE_CLOSE_HANDLE(HANDLE &h)
{
    XDBG("[Handle]close %p", h);
    CloseHandle(h);
    h = 0;
}
*/
inline unsigned long MyGetCurrentThreadID( ) 
{
#if defined(_WINDOWS_)
    return GetCurrentThreadId( ) ;
#else
    return pthread_self();
#endif
}

inline int randBetween(int min, int max)
{
    if (max==min) return min;
    unsigned int gap = abs(max-min);
    int ret = max>min?min:max;
    ret += rand()%(gap+1);
    return ret;
}

inline bool selectByPercent(int value)
{
    if(value>=randBetween(1,100))
        return true;
    return false;
}
inline bool selectByThousand(int value)
{
    if(value>=randBetween(1,1000))
        return true;
    return false;
}
inline bool selectByTenThousand(int value)
{
    if(value>=randBetween(1,10000))
        return true;
    return false;
}

#define parseFormat(buffer, fmt) \
    va_list argptr;\
    va_start(argptr, fmt);\
    vsnprintf(buffer, sizeof(buffer), fmt, argptr);\
    va_end(argptr)

template<typename T, typename M>
inline void Clamp(T &val, const M &min, const M &max)
{
    if (val < min) { val = min; return; }
    if (val > max) { val = max; return; }
}

template<typename T>
inline T square(const T &val)
{
    return val * val;
}

template<typename T>
inline T cube(const T &val)
{
    return val * val * val;
}

struct Parser
{
    Parser()
    {
        map.clear();
    }
    void reset()
    {
        for (std::map<std::string, std::string>::iterator it=map.begin(); it!=map.end(); it++)
            it->second.clear();
    }
    void key(std::string s)
    {
        std::string str("");
        map.insert(std::make_pair(s, str));
    }
    std::string value(std::string s)
    {
        return map[s];
    }
    std::map<std::string, std::string> map;
};

inline void getLocalTime(tm &_tm)
{
    time_t _t = time(NULL);
#ifndef _WINDOWS
    localtime_r(&_t, &_tm);
#else
    struct tm *tempTime = localtime(&_t);
    _tm = *tempTime;
#endif
}

inline void getLocalTime(tm &_tm, const time_t &_t)
{
#ifdef __LINUX__
    localtime_r(&_t, &_tm);
#else
    struct tm *tempTime = localtime(&_t);
    _tm = *tempTime;
#endif
}

inline void getLocalTime(tm &_tm, const UInt32 &_t)
{
#ifndef _WINDOWS
    localtime_r((time_t*)&_t, &_tm);
#else
    struct tm *tempTime = localtime((const time_t *)&_t);
    _tm = *tempTime;
#endif
}

inline void parseTime(const char *str, UInt32 &time)
{
    struct tm tm1;
    sscanf(str, "%4d-%2d-%2d %2d:%2d:%2d", &tm1.tm_year, &tm1.tm_mon, &tm1.tm_mday, &tm1.tm_hour, &tm1.tm_min, &tm1.tm_sec);
    tm1.tm_year -= 1900;
    tm1.tm_mon--;
    tm1.tm_isdst=-1;

    time = mktime(&tm1);
}


inline void addslashes(std::string &str)
{
    std::string::size_type p = str.find('\'');
    while (p != std::string::npos){
        str.replace(p, 1, "\\\'"); 
        p = str.find('\'', p + 2);
    }      
}

inline void addslashes(char *in, UInt16 len)
{
    std::string str(in, len);
    addslashes(str);
    strncpy(in, str.c_str(), len);
}

inline UInt32 abs(UInt32 value)
{
    return std::abs(long(value));
}

inline char* getReadableIP(UInt32 ip)
{
    struct in_addr tmp;
    tmp.s_addr = ip;
    return inet_ntoa(tmp);
}

const char* getLanguage(UInt32 id);

const char* printBinary(char* data,int dataLen);

template<typename T>
void _setValue(T &t, const UInt8* data)
{
    t = *((T*)data);
}

template<typename T>
void _setValue(T *t, const UInt8* data)
{
    *t = *((T*)data);
}

template<typename T>
void _getValue(T t, UInt8* data)
{
    *((T*)data) = t;
}


#ifdef _WINDOWS
int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, register int n);
#endif




#ifdef _WINDOWS
void* __builtin_return_address (int iLevel);
#endif