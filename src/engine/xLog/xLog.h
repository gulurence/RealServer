#pragma once

#include "xBase/xDefine.h"

#include <stdarg.h>
#include <boost/serialization/singleton.hpp> 
#include <boost/format.hpp>
#include <spdlog/sinks/rotating_file_sink.h>

class xLog : public boost::serialization::singleton<xLog>
{
public:
    enum Level
    {
        ERR = 0,
        WARN,
        INFO,
        DEBUG,
        TRACE
    };

    class Imp;

public:
    void Init(const char* prop = "log4cxx.properties");
    void Print(const char* file, long line, const char* funtion, Level level, const char* format, ...);

public:
    std::shared_ptr<spdlog::logger> CreateServiceLog(const std::string &strServiceName);
    bool RemoveServiceLog(const std::string& strServiceName);

protected:
    xLog(void);
    ~xLog(void);

private:
    std::string service_name;
    std::shared_ptr<spdlog::logger> _logger;
};

#define  XTRC(...) xLog::get_mutable_instance().Print(__FILE__, __LINE__, __FUNCTION__, xLog::TRACE, __VA_ARGS__)
#define  XDBG(...) xLog::get_mutable_instance().Print(__FILE__, __LINE__, __FUNCTION__, xLog::DEBUG, __VA_ARGS__)
#define  XLOG(...) xLog::get_mutable_instance().Print(__FILE__, __LINE__, __FUNCTION__, xLog::INFO, __VA_ARGS__)
#define  XINF(...) xLog::get_mutable_instance().Print(__FILE__, __LINE__, __FUNCTION__, xLog::INFO, __VA_ARGS__)
#define  XWRN(...) xLog::get_mutable_instance().Print(__FILE__, __LINE__, __FUNCTION__, xLog::WARN, __VA_ARGS__)
#define  XERR(...) xLog::get_mutable_instance().Print(__FILE__, __LINE__, __FUNCTION__, xLog::ERR, __VA_ARGS__)

