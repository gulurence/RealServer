#include "xLog.h"
#include <stdarg.h>

xLog *srvLog = NULL;

void xLog::log(int logLevel, const char* format, ...) {
    if (logLevel < LOG_LEVEL_DEBUG)
        logLevel = LOG_LEVEL_DEBUG;
    if (logLevel > LOG_LEVEL_FATAL)
        logLevel = LOG_LEVEL_FATAL;
    va_list va;
    va_start(va, format);
    _log(logLevel, format, va);
    va_end(va);
}

void xLog::fatal(const char* format, ...) {
    va_list va;
    va_start(va, format);
    _log(LOG_LEVEL_FATAL, format, va);
    va_end(va);
}

void xLog::error(const char* format, ...) {
    va_list va;
    va_start(va, format);
    _log(LOG_LEVEL_ERROR, format, va);
    va_end(va);
}

void xLog::warn(const char* format, ...) {
    va_list va;
    va_start(va, format);
    _log(LOG_LEVEL_WARN, format, va);
    va_end(va);
}

void xLog::info(const char* format, ...) {
    if (getLevel() > LOG_LEVEL_INFO) {
        return;
    }
    
    va_list va;
    va_start(va, format);
    _log(LOG_LEVEL_INFO, format, va);
    va_end(va);
}

void xLog::debug(const char* format, ...) {
    if (getLevel() > LOG_LEVEL_DEBUG) {
        return;
    }
    
    va_list va;
    va_start(va, format);
    _log(LOG_LEVEL_DEBUG, format, va);
    va_end(va);
}

bool xLog::isDebug() {

#ifndef _WINDOWS
    return (m_logger->getLevel() == Level::getDebug());
#endif

    return true;
}

void xLog::_log(int logLevel, const char* format, va_list args) {
    char message[20480];
    size_t dataLen = vsnprintf(message, sizeof(message) - 1, format, args);
    if (dataLen >= sizeof(message) - 1) {


#ifndef _WINDOWS
        m_logger->warn(("truncating part of log message."));
#else
        printf(("truncating part of log message."));
#endif

    }
    switch (logLevel) {
    case LOG_LEVEL_DEBUG:
#ifndef _WINDOWS
        m_logger->debug((message));
#else
        printf("[DEBUG] %s \n", message);
#endif
        break;
    case LOG_LEVEL_INFO:
#ifndef _WINDOWS
        m_logger->info((message));
#else
        printf("[INFO] %s \n", message);
#endif
        break;
    case LOG_LEVEL_WARN:
#ifndef _WINDOWS
        m_logger->warn((message));
#else
        printf("[WARN] %s \n", message);
#endif
        break;
    case LOG_LEVEL_ERROR:
#ifndef _WINDOWS
        m_logger->error((message));
#else
        printf("[ERROR] %s \n", message);
#endif
        break;
    default:
        break;
    }
}

void xLog::config(const string& configFile) {

#ifndef _WINDOWS
    xml::DOMConfigurator::configureAndWatch(configFile);
#endif

}

int xLog::getLevel()
{

#ifndef _WINDOWS
    if (m_logger->getParent()->getLevel() == NULL) {
        return 0;
    }
    if (m_logger->getParent()->getLevel()->equals(Level::getDebug()))
        return LOG_LEVEL_DEBUG;
    else if (m_logger->getParent()->getLevel()->equals(Level::getInfo()))
        return LOG_LEVEL_INFO;
    else if (m_logger->getParent()->getLevel()->equals(Level::getWarn()))
        return LOG_LEVEL_WARN;
    else if (m_logger->getParent()->getLevel()->equals(Level::getError()))
        return LOG_LEVEL_ERROR;
    else
        return LOG_LEVEL_FATAL;
#endif
    return 0;
}

xLog::xLog(const char* moduleName) {

#ifndef _WINDOWS
    setlocale(LC_ALL, "");
    xLog::config("./log.xml");
    m_logger = Logger::getLogger((moduleName));
    m_bufSize = BUF_SIZE;
#endif

}

xLog::~xLog() 
{
}

bool xLog::init(const char* moduleName)
{
    return true;
}

void xLog::end()
{
    delete this;
}
