/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: CActorReportMgr.cpp
      Purpose:   Actor 监控上报模块实现

      订阅 cluster:actor:report:req 频道, 解析订阅请求刷新 m_monitors 上报标记
      Sweep 对存活 Actor 调 BuildReportJson 写 cluster:actor:report:{type}:{id}(TTL 15s), 清理超时/已卸载标记
  *****************************************************************/

#include "CActorReportMgr.h"

#include "xActor/ActorMgr.h"
#include "xRedis/RedisCli.h"
#include "xLog/xLog.h"

#include <ctime>

namespace
{

const char *kReportReqChannel = "cluster:actor:report:req";
const int64 kReportKeepAliveSec = 15; // 上报数据 TTL ("上报状态 15 秒重置")
const int64 kReportStopSec = 20;      // 无订阅多久后停止 ("20 秒重置上报标记")

std::string ReportKey(EActorType t, ActorID id)
{
    return "cluster:actor:report:" + std::to_string((int32)t) + ":" + std::to_string((int64)id);
}

// 从 JSON 里取 "key": 之后的第一个整数 (十进制, 可负; 无该键/非法返回 0)。
// WebPlatform 侧固定发 {"type":N,"id":M}, 此处做最小解析即可, 不引 JSON 库。
int64 ExtractIntField(const std::string &msg, const char *key)
{
    std::string needle = std::string("\"") + key + "\"";
    size_t pos = msg.find(needle);
    if (pos == std::string::npos)
    {
        return 0;
    }
    pos = msg.find(':', pos + needle.size());
    if (pos == std::string::npos)
    {
        return 0;
    }
    ++pos;
    while (pos < msg.size() && (msg[pos] == ' ' || msg[pos] == '\t'))
    {
        ++pos;
    }

    bool neg = false;
    if (pos < msg.size() && (msg[pos] == '-' || msg[pos] == '+'))
    {
        neg = (msg[pos] == '-');
        ++pos;
    }
    int64 val = 0;
    bool any = false;
    while (pos < msg.size() && msg[pos] >= '0' && msg[pos] <= '9')
    {
        val = val * 10 + (int64)(msg[pos] - '0');
        ++pos;
        any = true;
    }
    return any ? (neg ? -val : val) : 0;
}

bool ParseReportRequest(const std::string &msg, EActorType &outType, ActorID &outId)
{
    // 两个键都必须在场, 避免缺键时误把 0 当有效值
    if (msg.find("\"type\"") == std::string::npos || msg.find("\"id\"") == std::string::npos)
    {
        return false;
    }
    outType = (EActorType)ExtractIntField(msg, "type");
    outId = (ActorID)ExtractIntField(msg, "id");
    return true;
}

} // namespace

bool CActorReportMgr::Init(CRedisCli *cli)
{
    if (!cli)
    {
        XWRN("[ActorReport] init skipped — no shared Redis client");
        return false;
    }
    m_cli = cli;

    // 订阅连接与命令池隔离 (见 CRedisCli::Subscribe), 回调在独立订阅线程执行。
    bool ok = m_cli->Subscribe(kReportReqChannel,
                               [this](const std::string &, const std::string &msg)
                               { OnReportRequest(msg); });
    XLOG("[ActorReport] initialized: channel=%s subscribed=%d", kReportReqChannel, ok);
    return ok;
}

void CActorReportMgr::OnReportRequest(const std::string &msg)
{
    EActorType type = EActorType_None;
    ActorID id = 0;
    if (!ParseReportRequest(msg, type, id))
    {
        XWRN("[ActorReport] bad request: %s", msg.c_str());
        return;
    }

    std::lock_guard<std::mutex> lock(m_lock);
    m_monitors[{type, id}] = (int64)time(nullptr);
    XDBG("[ActorReport] monitor requested type=%d id=%lld", (int32)type, (int64)id);
}

void CActorReportMgr::Sweep()
{
    if (!m_cli)
    {
        return;
    }
    int64 now = (int64)time(nullptr);

    std::lock_guard<std::mutex> lock(m_lock);
    for (auto it = m_monitors.begin(); it != m_monitors.end();)
    {
        EActorType type = it->first.first;
        ActorID id = it->first.second;
        int64 lastSeen = it->second;

        // 页面关闭/断订阅超时 → 停止上报并清标记
        if (now - lastSeen > kReportStopSec)
        {
            XLOG("[ActorReport] stop reporting type=%d id=%lld (idle %llds)", (int32)type,
                 (int64)id, (int64)(now - lastSeen));
            it = m_monitors.erase(it);
            continue;
        }

        // Actor 已卸载 → 直接清标记
        CActor *actor = CActorMgr::instance()->Find(type, id);
        if (!actor)
        {
            it = m_monitors.erase(it);
            continue;
        }

        // 写上报结构到 Redis (JSON String + 15s TTL)
        std::string json = actor->BuildReportJson(now);
        m_cli->Set(ReportKey(type, id), json);
        m_cli->Expire(ReportKey(type, id), kReportKeepAliveSec);
        XDBG("[ActorReport] report type=%d id=%lld gid=%llu", (int32)type, (int64)id,
             (uint64)actor->GetGlobalId());
        ++it;
    }
}
