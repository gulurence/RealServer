#ifndef _NLOGGER_H_
#define _NLOGGER_H_

#include <string>

#include "xDefine.h"

#ifndef _WINDOWS
#include <log4cxx/logger.h>
#include <log4cxx/xml/domconfigurator.h>
using namespace log4cxx;
#endif
using namespace std;


class xLog {
public:
    enum { BUF_SIZE = 1024 };
    enum {
        LOG_LEVEL_DEBUG = 0,
        LOG_LEVEL_INFO = 1,
        LOG_LEVEL_WARN = 2,
        LOG_LEVEL_ERROR = 3,
        LOG_LEVEL_FATAL = 4
    };    
    xLog(const char* moduleName);
    virtual ~xLog();

    static void config(const string& configFile);
    void log(int logLevel, const char* format, ...);
    void fatal(const char* format, ...);
    void error(const char* format, ...);
    void warn(const char* format, ...);
    void info(const char* format, ...);
    void debug(const char* format, ...);
    int getLevel();    
    bool isDebug();
    void end(); //delete this
    bool init(const char* moduleName);

private:
#ifndef _WINDOWS
    LoggerPtr m_logger;
#endif

    int m_bufSize;
    void _log(int logLevel, const char* format, va_list args);
};

extern xLog *srvLog;

#define XLOG_SETSILENT // if (!xLog::setSilent() && srvLog) srvLog->silentOp()
#define XLOG_END if (srvLog) srvLog->end()
#define XTRC if (srvLog) srvLog->info
#define XDBG if (srvLog) srvLog->debug
#define XINF if (srvLog) srvLog->info
#define XWRN if (srvLog) srvLog->warn
#define XERR if (srvLog) srvLog->error
#define XFTL if (srvLog) srvLog->fatal
#define XLOG XINF


#endif

