#include <time.h>
#include "xExecutionTime.h"



xExecutionTime ExecutionTimeWrap::_et(1000);
xTimeTest::xTimeTest(const std::string& logName) : _logName(logName) {
    clock_gettime(CLOCK_REALTIME, &_tv);
}

xTimeTest::~xTimeTest() {
    //uint64 begin = (uint64)_tv.tv_sec * 1000000000L + _tv.tv_nsec;
    //clock_gettime(CLOCK_REALTIME, &_tv);
    //uint64 end = (uint64)_tv.tv_sec * 1000000000L + _tv.tv_nsec;
    //XDBG("[%s],%llu鐎甸偊鍠氶～?, _logName.c_str(), (end - begin) / 1000);
}

void xExecutionTime::inc(const std::string& func, const uint64 total, const bool& addFlag) {
    _stTimes& st = _times[func];
    ++st.__times;
    st.__total_time += total;
    st.__addFlag = addFlag;
}

void xExecutionTime::reset(const bool bForcePrint) {
    if (bForcePrint || _log_timer.timeUp()) {
        uint64 totalTime = 0;
        for (_Times_iter it = _times.begin(); it != _times.end(); ++it) {
            if (it->second.__times && it->second.__addFlag)
                totalTime += it->second.__total_time;
        }

        for (_Times_iter it = _times.begin(); it != _times.end(); ++it) {
            if (it->second.__times && !it->second.__addFlag) {
                //                 XDBG("[闁圭瑳鍡╂斀闁哄啫鐖煎Λ璺ㄧ磼閻旀椿鍚€]%05.2f%%, %s, %llu鐎甸偊鍠氶～? %u婵? %llu鐎甸偊鍠氶～?婵?, 
                //                     it->second.__total_time * 100.0f / totalTime, it->first.c_str(), 
                //                     it->second.__total_time / 1000L, it->second.__times, 
                //                     it->second.__total_time / 1000L / it->second.__times);
            }
        }
        //XDBG("[闁圭瑳鍡╂斀闁哄啫鐖煎Λ璺ㄧ磼閻旀椿鍚€]-------------------------------"); 
        for (_Times_iter it = _times.begin(); it != _times.end(); ++it) {
            if (it->second.__times && it->second.__addFlag) {
                //                 XDBG("[闁圭瑳鍡╂斀闁哄啫鐖煎Λ璺ㄧ磼閻旀椿鍚€]%05.2f%%, %s, %llu鐎甸偊鍠氶～? %u婵? %llu鐎甸偊鍠氶～?婵?, 
                //                         it->second.__total_time * 100.0f / totalTime, it->first.c_str(), 
                //                         it->second.__total_time / 1000L, it->second.__times, 
                //                         it->second.__total_time / 1000L / it->second.__times);
            }
        }
        //XDBG("[闁圭瑳鍡╂斀闁哄啫鐖煎Λ璺ㄧ磼閻旀椿鍚€]闁诡剛绮鍌炴⒒?%llu鐎甸偊鍠氶～?闂傚倹鎸冲▓?%u", totalTime / 1000L, _log_timer.getElapse());
        _times.clear();
    }
}

ExecutionTimeWrap::ExecutionTimeWrap(const std::string& func, bool addFlag) : _func(func), _addFlag(addFlag) {
    clock_gettime(CLOCK_REALTIME, &_tv);
}

ExecutionTimeWrap::~ExecutionTimeWrap() {
    uint64 begin = (uint64)_tv.tv_sec * 1000000000L + _tv.tv_nsec;

    clock_gettime(CLOCK_REALTIME, &_tv);

    uint64 end = (uint64)_tv.tv_sec * 1000000000L + _tv.tv_nsec;

    _et.inc(_func, end - begin, _addFlag);
}








