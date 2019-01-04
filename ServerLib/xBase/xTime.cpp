#include "xTime.h"
#include <time.h>
#ifndef _WINDOWS
#include <sys/time.h>
#else
#include <time.h>
#endif
#include "xTools.h"

xTime::xTime()
{
    now();
    _elapse = usec;
}

xTime::~xTime()
{
}

void xTime::now()
{
    usec = getCurUSec();
}

UInt32 xTime::getCurSec()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return tv.tv_sec;
}

UInt64 xTime::getCurUSec()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return ((UInt64)tv.tv_sec)*1000000+tv.tv_usec;
}

time_t xTime::getDayStart(time_t time)
{
    struct tm tm;
    getLocalTime(tm, time);
    return time - (tm.tm_hour * 60 + tm.tm_min) * 60 - tm.tm_sec;//直接计算比使用mktime函数效率更高
}

time_t xTime::getDayStart(UInt32 year, UInt32 month, UInt32 day)
{
    if (year < 1970 || month < 1 || month > 12 || day > 31)
        return 0;

    struct tm tm;
    bzero(&tm, sizeof(tm));
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    return mktime(&tm);
}

//以周一早上0点为起始时间
time_t xTime::getWeekStart(time_t time)
{
    struct tm tm;
    getLocalTime(tm, time);
    return time - ((((tm.tm_wday + 6) % 7) * 24 + tm.tm_hour) * 60 + tm.tm_min) * 60 - tm.tm_sec;
}

time_t xTime::getMonthStart(time_t time)
{
    struct tm tm;
    getLocalTime(tm, time);
    return time - (((tm.tm_mday - 1) * 24 + tm.tm_hour) * 60 + tm.tm_min) * 60 - tm.tm_sec;
}

const UInt32 MONTH_DAY_NUM[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
bool xTime::isValidDate(UInt32 year, UInt32 month, UInt32 day)
{
    if (month < 1 || month > 12 || day < 1 || day > 31)
        return false;

    return day <= MONTH_DAY_NUM[month] + ((month == 2 && isLeapYear(year)) ? 1 : 0);
}

bool xTime::isLeapYear(UInt32 year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

bool xTime::isSameDay(time_t t1, time_t t2)
{
    return getDayStart(t1)==getDayStart(t2);
}
bool xTime::isSameWeek(time_t t1,time_t t2)
{
    return getWeekStart(t1)==getWeekStart(t2); 
}
bool xTime::isSameMonth(time_t t1,time_t t2)
{
    return getMonthStart(t1)==getMonthStart(t2);
}

int xTime::getDay(time_t time)
{
    struct tm tm;
    getLocalTime(tm, time);
    return tm.tm_mday;
}

int xTime::getMonth(time_t time)
{
    struct tm tm;
    getLocalTime(tm, time);
    return tm.tm_mon+1;
}

int xTime::getYear(time_t time)
{
    struct tm tm;
    getLocalTime(tm, time);
    return tm.tm_year+1900;
}

int xTime::getWeek(time_t time)
{
    struct tm tm;
    getLocalTime(tm, time);
    return tm.tm_wday;
}

int xTime::getHour(time_t time)
{
    struct tm tm;
    getLocalTime(tm, time);
    return tm.tm_hour;
}

int xTime::getMin(time_t time)
{
    struct tm tm;
    getLocalTime(tm, time);
    return tm.tm_min;
}

void xTime::elapseStart()
{
    _elapse = getCurUSec();
}

UInt32 xTime::elapse()
{
    return (UInt32)(uElapse()/1000000);
}

bool xTime::elapse(UInt32 s)
{
    if (elapse()>=s)
    {
        elapseStart();
        return true;
    }
    return false;
}

UInt64 xTime::milliElapse()
{
    return uElapse()/1000;
}

bool xTime::milliElapse(UInt64 m)
{
    if (milliElapse()>=m)
    {
        elapseStart();
        return true;
    }
    return false;
}

UInt64 xTime::uElapse()
{
    UInt64 cur = getCurUSec();

    if (cur>=_elapse)
        return cur-_elapse;
    else
        return 0;
}

bool xTime::uElapse(UInt64 u)
{
    if (uElapse()>=u)
    {
        elapseStart();
        return true;
    }
    return false;
}

std::string getAscTime(const UInt32 &dwTime)
{
    char str[128];
    bzero(str, sizeof(str));

    struct tm tm;
    getLocalTime(tm, (time_t)time);

    snprintf(str, sizeof(str), "%u-%u-%u %u:%u", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);

    return str;
}

#ifdef _WINDOWS

int gettimeofday(struct timeval *tp, void *tzp)
{
    time_t clock;
    struct tm tm;
    SYSTEMTIME wtm;

    GetLocalTime(&wtm);
    tm.tm_year     = wtm.wYear - 1900;
    tm.tm_mon     = wtm.wMonth - 1;
    tm.tm_mday     = wtm.wDay;
    tm.tm_hour     = wtm.wHour;
    tm.tm_min     = wtm.wMinute;
    tm.tm_sec     = wtm.wSecond;
    tm. tm_isdst    = -1;
    clock = mktime(&tm);
    tp->tv_sec = clock;
    tp->tv_usec = wtm.wMilliseconds * 1000;

    return (0);
}
#endif

