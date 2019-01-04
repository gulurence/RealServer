#ifndef _X_EXECUTIONTIME_H_
#define _X_EXECUTIONTIME_H_

#include "xTime.h"
#include "xDefine.h"
#include <map>
#include <string>
#include "pthread.h"

class xTimeTest    //简单的执行时间测试函数
{
    public:
        xTimeTest(const std::string &logName);
        ~xTimeTest();
    private:
        std::string _logName;
        struct timespec _tv;
};

class xExecutionTime
{
    public:
        xExecutionTime(const UInt32 &interval) : _log_timer(interval) {}
        ~xExecutionTime() {}
        void inc(const std::string &func, const UInt64 total, const bool &addFlag);
        void reset(const bool force_print);

    private:
        struct _stTimes
        {
            UInt32 __times;    //次数
            UInt64 __total_time;    //总耗时
            bool __addFlag;
            _stTimes() { bzero(this, sizeof(*this)); }
        };
        typedef std::map<std::string, _stTimes> _Times;
        typedef _Times::iterator _Times_iter;
        _Times _times;
        xTimer _log_timer;
};

class ExecutionTimeWrap
{
    public:
        ExecutionTimeWrap(const std::string &func, bool addFlag = true);
        ~ExecutionTimeWrap();

        static xExecutionTime _et;

    private:
        const std::string _func;
        struct timespec _tv;
        bool _addFlag;
};

#define ExecutionTime_setInterval(intv)\
    xExecutionTime ExecutionTimeWrap::_et(intv);

#define ExecutionTime_Scope\
    ExecutionTimeWrap _exec_time_scope("LINE:"_S(__LINE__))

#define ExecutionTime_Scope_NoAdd\
    ExecutionTimeWrap _exec_time_scope("LINE:"_S(__LINE__), false)

#define ExecutionTime_Reset(flag)\
    ExecutionTimeWrap::_et.reset(flag);

#endif
