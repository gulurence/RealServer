#pragma once

#include "xDefine.h"

#include <stdarg.h>
#include <boost/serialization/singleton.hpp> 
#include <boost/format.hpp>
#include <log4cxx/logger.h>  
#include <log4cxx/basicconfigurator.h>  
#include <log4cxx/propertyconfigurator.h>  
#include <log4cxx/helpers/exception.h>

class Logcxx : public boost::serialization::singleton<Logcxx>
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
    void Init(const char* prop);
    void Print(const char* file, long line, const char* funtion, Level level, const char* format, ...);

protected:
    Logcxx(void);
    ~Logcxx(void);

private:
    Imp* _pImp;
};

class Logcxx::Imp
{
public:
    Imp() {}
    ~Imp() {}

    void Init(const char* prop = "log4cxx.properties");
    void Print(Logcxx::Level level, std::string& buff);
};

#define  XTRC(...) Logcxx::get_mutable_instance().Print(__FILE__, __LINE__, __FUNCTION__, Logcxx::TRACE, __VA_ARGS__)
#define  XDBG(...) Logcxx::get_mutable_instance().Print(__FILE__, __LINE__, __FUNCTION__, Logcxx::DEBUG, __VA_ARGS__)
#define  XLOG(...) Logcxx::get_mutable_instance().Print(__FILE__, __LINE__, __FUNCTION__, Logcxx::INFO, __VA_ARGS__)
#define  XINF(...) Logcxx::get_mutable_instance().Print(__FILE__, __LINE__, __FUNCTION__, Logcxx::INFO, __VA_ARGS__)
#define  XWRN(...) Logcxx::get_mutable_instance().Print(__FILE__, __LINE__, __FUNCTION__, Logcxx::WARN, __VA_ARGS__)
#define  XERR(...) Logcxx::get_mutable_instance().Print(__FILE__, __LINE__, __FUNCTION__, Logcxx::ERR, __VA_ARGS__)


