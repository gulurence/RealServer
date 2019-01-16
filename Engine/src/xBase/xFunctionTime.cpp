#include <time.h>
#include "xFunctionTime.h"
#include "xLog.h"

xFunctionTime FunctionTimeWrap::_ft(1000);
void xFunctionTime::inc(const uint64 &addr, const uint64 &total)
{
    _stTimes &st = _times[addr];
    ++st.__times;
    st.__total_time += total;
}

void xFunctionTime::reset(const bool force_print)
{
    if ((force_print || _log_timer.timeUp()) && !_times.empty())
    {
        XDBG("[函数时间统计]%s,间隔:%u,大小:%u", force_print ? "强制" : "时间", _log_timer.getElapse(), _times.size());
        for (_Times_iter it = _times.begin(); it != _times.end(); ++it)
        {
            if (it->second.__times)
            {
                /*
                XDBG("[函数时间统计]地址:%llu, %llu毫秒, %u次, %u毫秒/次", it->first, 
                        it->second.__total_time / 1000000L, it->second.__times, 
                        it->second.__total_time / 1000000L / it->second.__times);
                // */
                XDBG("[函数时间统计]地址:%llx, %llu微秒, %u次, %u微秒/次 ", it->first, 
                        it->second.__total_time / 1000L, it->second.__times, 
                        it->second.__total_time / 1000L / it->second.__times);
            }
        }
        _times.clear();
    }
}

FunctionTimeWrap::FunctionTimeWrap(const uint64 &addr) : _addr(addr)
{
    clock_gettime(CLOCK_REALTIME, &_tv);
}

FunctionTimeWrap::~FunctionTimeWrap()
{
    uint64 begin = (uint64)_tv.tv_sec * 1000000000L + _tv.tv_nsec;

    clock_gettime(CLOCK_REALTIME, &_tv);

    uint64 end = (uint64)_tv.tv_sec * 1000000000L + _tv.tv_nsec;

    _ft.inc(_addr, end - begin);
}

