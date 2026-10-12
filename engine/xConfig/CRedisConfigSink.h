// =============================================================================
// CRedisConfigSink.h — 配置变更 Redis 发布端 (C3)
//
// 把共享核心 rsconfig::ConfigChange 序列化为 JSON, 经 CRedisCli::Publish 广播到
// 配置热更频道。服务器侧把本类设为 rsconfig::CConfigMgr 的 sink, 每次 Load 提交后
// 自动发布; 订阅端(客户端/其他进程)走 CRedisCli::Subscribe 收到后动态拉取 (C4)。
//
// 频道协议 (JSON): {"version":<u32>,"contentHash":<u64>,"tables":["sp01",...]}
// =============================================================================
#pragma once

#include "config/CConfigMgr.h"
#include "xRedis/RedisCli.h"

#include <string>

// 配置变更 Redis 发布端 (发布即可, 无需引擎类型别名; 只碰 rsconfig 载荷 + CRedisCli 接口)
class CRedisConfigSink : public rsconfig::IConfigChangeSink
{
public:
    static constexpr const char *kChannel = "config:changed";

    explicit CRedisConfigSink(CRedisCli *cli) : m_cli(cli)
    {
    }

    void OnConfigChanged(const rsconfig::ConfigChange &ch) override
    {
        if (m_cli)
        {
            m_cli->Publish(kChannel, ToJson(ch));
        }
    }

    // 载荷 → JSON (表名/路径来自我方导出器, 只做最小转义)
    static std::string ToJson(const rsconfig::ConfigChange &ch)
    {
        std::string s = "{\"version\":" + std::to_string(ch.version)
                        + ",\"contentHash\":" + std::to_string(ch.contentHash) + ",\"tables\":[";
        for (size_t i = 0; i < ch.changedTables.size(); ++i)
        {
            if (i)
            {
                s += ",";
            }
            s += "\"" + Escape(ch.changedTables[i]) + "\"";
        }
        s += "]}";
        return s;
    }

private:
    static std::string Escape(const std::string &in)
    {
        std::string out;
        out.reserve(in.size());
        for (char c : in)
        {
            switch (c)
            {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            default:
                out += c;
            }
        }
        return out;
    }

    CRedisCli *m_cli;
};
