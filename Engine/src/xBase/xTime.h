#pragma once
#include <time.h>
#include "xDefine.h"

#ifdef _WINDOWS
#define clock_gettime(CLOCK_REALTIME, _tv) {\
(_tv)->tv_sec = clock()/1000;\
(_tv)->tv_nsec = clock()%1000*1000000L;}

// struct timespec
// {
//     time_t tv_sec; /*second*/ 
//     long tv_nsec;/*nanosecond*/ 
// };

int gettimeofday(struct timeval *tp, void *tzp);
#endif



class xTime
{
    public:
        xTime();
        ~xTime();

        static uint64 getCurUSec();
        static uint32 getCurSec();
        static time_t    getDayStart(time_t time);
        static time_t    getDayStart(uint32 year, uint32 month, uint32 day);
        static time_t    getWeekStart(time_t time);
        static time_t    getMonthStart(time_t time);
        static bool        isSameDay(time_t t1, time_t t2);
        static bool        isSameWeek(time_t t1,time_t t2);
        static bool        isSameMonth(time_t t1,time_t t2);
        static bool        isValidDate(uint32 year, uint32 month, uint32 day);    //检查日期是否合法
        static bool        isLeapYear(uint32 year);
        static int        getDay(time_t time);
        static int        getMonth(time_t time);
        static int        getYear(time_t time);
        static int        getWeek(time_t time);
        static int        getHour(time_t time);
        static int        getMin(time_t time);

        void now();

        void elapseStart();

        uint32 elapse();
        bool elapse(uint32 cur);
        uint64 milliElapse();
        bool milliElapse(uint64 cur);
        uint64 uElapse();
        bool uElapse(uint64 cur);

        uint64 usec;
        uint64 _elapse;
};

class xTimer
{
    public:
        xTimer(uint32 t)
        {
            elapse = t;
        }
        ~xTimer(){}
        inline bool timeUp()
        {
            return time.elapse(elapse);
        }
        void reset()
        {
            time.elapseStart();
        }
        inline uint32 getElapse() const { return elapse; }
    private:
        xTime time;
        uint32 elapse;
};

class xMilliTimer
{
    public:
        xMilliTimer(uint32 t)
        {
            elapse = t;
        }
        ~xMilliTimer(){}
        bool timeUp()
        {
            return time.milliElapse(elapse);
        }
        void reset()
        {
            time.elapseStart();
        }
    private:
        xTime time;
        uint32 elapse;
};

//定时器  以天为周期  可设置每天几点执行一次
class xDayTimer
{
    public:
        xDayTimer(uint8 h, uint8 d=5)
        {
            _hour = h;

            _last_done = 0;

            _delay = d;
        }
        ~xDayTimer()
        {
        }
        bool timeUp(struct tm &tm_cur)
        {
            if (_last_done != tm_cur.tm_mday)
            {
                if (tm_cur.tm_hour == _hour)
                {
                    if (tm_cur.tm_min <= _delay)
                    {
                        _last_done = tm_cur.tm_mday;
                        return true;
                    }
                    _last_done = tm_cur.tm_mday;
                }
                return false;
            }
            return false;
        }
    private:
        uint8 _hour;

        uint8 _last_done;

        uint8 _delay;
};

std::string getAscTime(const uint32 &dwTime);
