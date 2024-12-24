#include "xLog.h"
#include <string.h>

Logcxx::Logcxx(void) {
    _pImp = new Imp();
}

Logcxx::~Logcxx(void) {
    if (_pImp) {
        delete _pImp;
    }
}

void Logcxx::Init(const char* prop) {
    _pImp->Init(prop);
}

void Logcxx::Print(const char* file, long line, const char* funtion, Level level, const char* format, ...) {
    char buff[4096] = { 0 };
    va_list ap;
    va_start(ap, format);
    vsnprintf_s(buff, 4096, format, ap);
    va_end(ap);

    std::string strlog = (boost::format(" %s %s %s : %s") % file % line % funtion % buff).str();

    _pImp->Print(level, strlog);
}

void Logcxx::Imp::Init(const char* prop) {
    setlocale(LC_ALL, "");
    log4cxx::PropertyConfigurator::configureAndWatch(prop);
    log4cxx::LoggerPtr rootLogger = log4cxx::Logger::getRootLogger();
    assert(rootLogger);
}

void Logcxx::Imp::Print(Logcxx::Level level, std::string& buff) {
    log4cxx::LoggerPtr rootLogger(log4cxx::Logger::getRootLogger());
    switch (level) {
    case Logcxx::ERR:
        rootLogger->error(buff);
        break;
    case Logcxx::WARN:
        rootLogger->warn(buff);
        break;
    case Logcxx::DEBUG:
        rootLogger->debug(buff);
        break;
    case Logcxx::INFO:
        rootLogger->info(buff);
        break;
    case Logcxx::TRACE:
        rootLogger->trace(buff);
        break;
    default:
        assert(false);
        rootLogger->info(buff);
        break;
    }
}



