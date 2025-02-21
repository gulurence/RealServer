#pragma once

#include "xTime.h"
#include "xTools.h"

class xFunctionTime
{
    public:
        xFunctionTime(const uint32 &interval) : _log_timer(interval) {}
        ~xFunctionTime() {}
        void inc(const uint64 &addr, const uint64 &total);
        void reset(const bool force_print);

    private:
        struct _stTimes
        {
            uint32 __times;    //婵炲棌鍓濋弳?
            uint64 __total_time;    //闁诡剚妲掗埀顒侇殕濡?
            _stTimes() { bzero(this, sizeof(*this)); }
        };
        typedef std::map<uint64, _stTimes> _Times;
        typedef _Times::iterator _Times_iter;
        _Times _times;

        xTimer _log_timer;
};

class FunctionTimeWrap
{
    public:
        FunctionTimeWrap(const uint64 &addr);
        ~FunctionTimeWrap();

        static xFunctionTime _ft;

    private:
        const uint64 _addr;
        struct timespec _tv;
};

#define FunctionTime_setInterval(intv)\
    xFunctionTime FunctionTimeWrap::_ft(intv);

#define FunctionTime_Scope\
    FunctionTimeWrap _func_time_scope(reinterpret_cast<uint64>(__builtin_return_address(0)))

#define FunctionTime_Reset(flag)\
    FunctionTimeWrap::_ft.reset(flag);



