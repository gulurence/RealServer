#pragma once

#include "xTime.h"
#include "xDefine.h"


class xTimeTest    //缂佺姭鍋撻柛妤佹礈濞堟垿骞嶈椤㈡垿寮崼鏇燂紵婵炴潙顑堥惁顖炲礄閼恒儲娈?
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
        xExecutionTime(const uint32 &interval) : _log_timer(interval) {}
        ~xExecutionTime() {}
        void inc(const std::string &func, const uint64 total, const bool &addFlag);
        void reset(const bool force_print);

    private:
        struct _stTimes
        {
            uint32 __times;    //婵炲棌鍓濋弳?
            uint64 __total_time;    //闁诡剚妲掗埀顒侇殕濡?
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



