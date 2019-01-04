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
    //return (m_logger->getLevel() == Level::getDebug());
    return true;
}

void xLog::_log(int logLevel, const char* format, va_list args) {
    char message[20480];
    size_t dataLen = vsnprintf(message, sizeof(message) - 1, format, args);
    if (dataLen >= sizeof(message) - 1) {
        //m_logger->warn(("truncating part of log message."));
        printf(("truncating part of log message."));
    }
    switch (logLevel) {
    case LOG_LEVEL_DEBUG:
        //m_logger->debug((message));
        printf("[DEBUG] %s \n", message);
        break;
    case LOG_LEVEL_INFO:
        //m_logger->info((message));
        printf("[INFO] %s \n", message);
        break;
    case LOG_LEVEL_WARN:
       // m_logger->warn((message));
        printf("[WARN] %s \n", message);
        break;
    case LOG_LEVEL_ERROR:
        //m_logger->error((message));
        printf("[ERROR] %s \n", message);
        break;
    default:
        break;
    }
}

void xLog::config(const string& configFile) {
    //xml::DOMConfigurator::configureAndWatch(configFile);
}

int xLog::getLevel()
{
//     if(m_logger->getParent()->getLevel() == NULL ){
//         return 0;
//     }
//     if(m_logger->getParent()->getLevel()->equals(Level::getDebug()))
//         return LOG_LEVEL_DEBUG;
//     else if (m_logger->getParent()->getLevel()->equals(Level::getInfo()))
//         return LOG_LEVEL_INFO;
//     else if (m_logger->getParent()->getLevel()->equals(Level::getWarn()))
//         return LOG_LEVEL_WARN;
//     else if (m_logger->getParent()->getLevel()->equals(Level::getError()))
//         return LOG_LEVEL_ERROR;
//     else 
//         return LOG_LEVEL_FATAL;
    return 0;
}

xLog::xLog(const char* moduleName) {
//  setlocale(LC_ALL, "");
//  xLog::config("./log.xml");
//     m_logger = Logger::getLogger((moduleName));
//     m_bufSize = BUF_SIZE;
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
