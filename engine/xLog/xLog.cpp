/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: xLog.cpp
      Purpose:   spdlog 日志封装实现

      实现 xLog::Init（rotating file + stdout 双 sink、秒级异步 flush）与 Print
      按 Level 分发到 spdlog，输出带文件名与行号前缀
  *****************************************************************/

#include "xLog.h"

#include <spdlog/spdlog.h>
#include "spdlog/sinks/stdout_color_sinks.h"

#include <chrono>

xLog::xLog(void)
{
}

xLog::~xLog(void)
{
}

void xLog::Init(const char *prop)
{
    service_name = "normal";
    std::string logFileName = "logs/" + service_name + ".log";
    // 文件日志
    auto sink
        = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logFileName, 1048576 * 5, 400);
    // sink->set_pattern("[%Y-%m-%d %H:%M:%S] [%^%L%$] %v");
    //  前台日志
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    // console_sink->set_pattern("[%Y-%m-%d %H:%M:%S] [%^%L%$] %v");
    //
    spdlog::set_pattern("[%H:%M:%S %z] %v");
    // 设置日志
    std::vector<spdlog::sink_ptr> sinks = {sink, console_sink};
    _logger = std::make_shared<spdlog::logger>(service_name, sinks.begin(), sinks.end());
    spdlog::set_default_logger(_logger);

    // 周期性异步冲刷 (spdlog 后台线程每 1s flush 一次), 替代逐条日志同步 flush。
    // 逐条 flush 是登录/RPC 热路径的无谓磁盘/文件缓冲同步开销, 改为秒级 flush 后
    // 日志仍能及时落盘, 但日志调用本身不再阻塞等待落盘。
    spdlog::flush_every(std::chrono::seconds(1));

    XLOG("[INIT] xLog initialized — console + rotating file: %s", logFileName.c_str());

    // 文件日志
    // auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logFileName, 1048576 * 5,
    // 400); auto logger = std::make_shared<spdlog::logger>("logger", sink);
    // spdlog::set_default_logger(logger);
}

std::shared_ptr<spdlog::logger> xLog::CreateServiceLog(const std::string &strServiceName)
{
    std::string logFileName = "logs/" + strServiceName + "/" + strServiceName + ".log";
    // 文件日志
    auto sink
        = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logFileName, 1048576 * 5, 400);
    auto logger = std::make_shared<spdlog::logger>(strServiceName, sink);
    return logger;
}

bool xLog::RemoveServiceLog(const std::string &strServiceName)
{

    return true;
}

void xLog::Print(const char *file, int32 line, const char *funtion, Level level, const char *format,
                 ...)
{
    char buff[4096] = {0};
    va_list ap;
    va_start(ap, format);
    vsnprintf(buff, 4096, format, ap);
    va_end(ap);

    auto filePatch = std::string(file);
    std::string::size_type pos = filePatch.rfind('\\');
    std::string::size_type pos2 = filePatch.rfind('/');
    if (pos2 != std::string::npos && (pos == std::string::npos || pos2 > pos))
    {
        pos = pos2;
    }
    if (pos != std::string::npos)
    {
        filePatch = filePatch.substr(pos + 1);
    }

    char fmtBuf[4352] = {0};
    snprintf(fmtBuf, sizeof(fmtBuf), " %s:%ld - %s", filePatch.c_str(), line, buff);
    std::string strlog = fmtBuf;

    switch (level)
    {
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
}
