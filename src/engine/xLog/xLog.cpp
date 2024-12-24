#include "xLog.h"

#include <spdlog/spdlog.h>
#include "spdlog/sinks/stdout_color_sinks.h"

xLog::xLog(void) {
}

xLog::~xLog(void) {
}

void xLog::Init(const char* prop) {
    service_name = "normal_service";
    std::string logFileName = "logs/" + service_name + ".log";
    // 文件日志
    auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logFileName, 1048576 * 5, 400);
    //sink->set_pattern("[%Y-%m-%d %H:%M:%S] [%^%L%$] %v");
    // 前台日志
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    //console_sink->set_pattern("[%Y-%m-%d %H:%M:%S] [%^%L%$] %v");
    //
    spdlog::set_pattern("[%H:%M:%S %z] %v");
    // 设置日志
    std::vector<spdlog::sink_ptr> sinks = { sink, console_sink };
    _logger = std::make_shared<spdlog::logger>(service_name, sinks.begin(), sinks.end());
    spdlog::set_default_logger(_logger);
    spdlog::set_level(spdlog::level::info);
}

void xLog::Print(const char* file, long line, const char* funtion, Level level, const char* format, ...) {
    char buff[4096] = { 0 };
    va_list ap;
    va_start(ap, format);
    vsnprintf_s(buff, 4096, format, ap);
    va_end(ap);

    auto filePatch = std::string(file);
    std::string::size_type first = filePatch.rfind('\\');
    if (first != std::string::npos) {
        filePatch = filePatch.substr(first + 1, filePatch.size());
    }

    std::string strlog = (boost::format(" %s:%s - %s") % filePatch % line % buff).str();

    switch (level) {
    case Level::ERR:
        _logger->error(strlog);
        break;
    case Level::WARN:
        _logger->warn(strlog);
        break;
    case Level::DEBUG:
        _logger->debug(strlog);
        break;
    case Level::INFO:
        _logger->info(strlog);
        break;
    case Level::TRACE:
        _logger->trace(strlog);
        break;
    default:
        assert(false);
        _logger->error(strlog);
        break;
    }
    _logger->flush();
}

