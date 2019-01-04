#include <time.h>
#include "xExecutionTime.h"



xExecutionTime ExecutionTimeWrap::_et(1000);
xTimeTest::xTimeTest(const std::string &logName) : _logName(logName)
{
    clock_gettime(CLOCK_REALTIME, &_tv);
}

xTimeTest::~xTimeTest()
{
    //UInt64 begin = (UInt64)_tv.tv_sec * 1000000000L + _tv.tv_nsec;
    //clock_gettime(CLOCK_REALTIME, &_tv);
    //UInt64 end = (UInt64)_tv.tv_sec * 1000000000L + _tv.tv_nsec;
    //XDBG("[%s],%llu微秒", _logName.c_str(), (end - begin) / 1000);
}

void xExecutionTime::inc(const std::string &func, const UInt64 total, const bool &addFlag)
{
    _stTimes &st = _times[func];
    ++st.__times;
    st.__total_time += total;
    st.__addFlag = addFlag;
}

void xExecutionTime::reset(const bool force_print)
{
    if (force_print || _log_timer.timeUp())
    {
        UInt64 totalTime = 0;
        //XDBG("[执行时间统计]%s,间隔:%u,大小:%u", force_print ? "强制" : "时间", _log_timer.getElapse(), _times.size());
        for (_Times_iter it = _times.begin(); it != _times.end(); ++it)
        {
            if (it->second.__times && it->second.__addFlag)
                totalTime += it->second.__total_time;
        }

        for (_Times_iter it = _times.begin(); it != _times.end(); ++it)
        {
            if (it->second.__times && !it->second.__addFlag)
            {
//                 XDBG("[执行时间统计]%05.2f%%, %s, %llu微秒, %u次, %llu微秒/次", 
//                     it->second.__total_time * 100.0f / totalTime, it->first.c_str(), 
//                     it->second.__total_time / 1000L, it->second.__times, 
//                     it->second.__total_time / 1000L / it->second.__times);
            }
        }
        //XDBG("[执行时间统计]-------------------------------"); 
        for (_Times_iter it = _times.begin(); it != _times.end(); ++it)
        {
            if (it->second.__times && it->second.__addFlag)
            {
//                 XDBG("[执行时间统计]%05.2f%%, %s, %llu微秒, %u次, %llu微秒/次", 
//                         it->second.__total_time * 100.0f / totalTime, it->first.c_str(), 
//                         it->second.__total_time / 1000L, it->second.__times, 
//                         it->second.__total_time / 1000L / it->second.__times);
            }
        }
        //XDBG("[执行时间统计]总时间:%llu微秒,间隔:%u", totalTime / 1000L, _log_timer.getElapse());
        _times.clear();
    }
}

ExecutionTimeWrap::ExecutionTimeWrap(const std::string &func, bool addFlag) : _func(func), _addFlag(addFlag)
{
    clock_gettime(CLOCK_REALTIME, &_tv);
}

ExecutionTimeWrap::~ExecutionTimeWrap()
{
    UInt64 begin = (UInt64)_tv.tv_sec * 1000000000L + _tv.tv_nsec;

    clock_gettime(CLOCK_REALTIME, &_tv);

    UInt64 end = (UInt64)_tv.tv_sec * 1000000000L + _tv.tv_nsec;

    _et.inc(_func, end - begin, _addFlag);
}

