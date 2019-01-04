#ifndef _X_FUNCTIONTIME_H_
#define _X_FUNCTIONTIME_H_

#include "xTime.h"
#include "xDefine.h"
#include <map>
#include <string>
#include "pthread.h"
#include "xTools.h"

class xFunctionTime
{
    public:
        xFunctionTime(const UInt32 &interval) : _log_timer(interval) {}
        ~xFunctionTime() {}
        void inc(const UInt64 &addr, const UInt64 &total);
        void reset(const bool force_print);

    private:
        struct _stTimes
        {
            UInt32 __times;    //次数
            UInt64 __total_time;    //总耗时
            _stTimes() { bzero(this, sizeof(*this)); }
        };
        typedef std::map<UInt64, _stTimes> _Times;
        typedef _Times::iterator _Times_iter;
        _Times _times;

        xTimer _log_timer;
};

class FunctionTimeWrap
{
    public:
        FunctionTimeWrap(const UInt64 &addr);
        ~FunctionTimeWrap();

        static xFunctionTime _ft;

    private:
        const UInt64 _addr;
        struct timespec _tv;
};

#define FunctionTime_setInterval(intv)\
    xFunctionTime FunctionTimeWrap::_ft(intv);

#define FunctionTime_Scope\
    FunctionTimeWrap _func_time_scope(reinterpret_cast<UInt64>(__builtin_return_address(0)))

#define FunctionTime_Reset(flag)\
    FunctionTimeWrap::_ft.reset(flag);

#endif
