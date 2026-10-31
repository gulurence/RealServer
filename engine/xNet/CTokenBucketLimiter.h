/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: CTokenBucketLimiter.h
      Purpose:   单连接消息接收限流器：每 CID 一个令牌桶的匀速补充限流。

      定义 CTokenBucketLimiter 类：SetRate 设置速率并清空旧桶，Consume 每秒补充 rate 枚令牌、放行扣 1 枚
      桶容量等于 rate，允许一次突发，rate<=0 不限流，Reset 断线时清理该 CID 桶
      std::map<NetCID,Bucket> 加 m_mutex 保证 PackageCheck 并发调用线程安全
  *****************************************************************/

#pragma once

#include "xNetDefine.h"

#include <chrono>
#include <map>
#include <mutex>

// =============================================================================
// CTokenBucketLimiter — 单连接消息接收限流器 (令牌桶)
//
// 每个 CID 一个令牌桶: 容量 = rate (允许一次突发 rate 条), 每秒匀速补充 rate 枚
// 令牌。Consume 放行则扣 1 枚令牌; 桶空返回 false (超限, 调用方应断开连接)。
// rate <= 0 表示不限流。线程安全 (PackageCheck 每连接一线程并发调用)。
// =============================================================================
class CTokenBucketLimiter
{
public:
    // 设置限流速率 (消息/秒); rate <= 0 视为不限流。速率变更时清空既有桶,
    // 避免旧速率下残留令牌被瞬时释放。
    void SetRate(int32 rate)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_rate = rate < 0 ? 0 : rate;
        m_buckets.clear();
    }

    // 尝试消费一枚令牌 (返回是否放行; false = 超限)
    bool Consume(NetCID cid)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_rate <= 0)
        {
            return true;
        }

        Bucket &b = m_buckets[cid];
        auto now = std::chrono::steady_clock::now();
        if (b.last.time_since_epoch().count() == 0)
        {
            // 首包: 给满一桶 (允许一次突发 rate 条)
            b.tokens = (double)m_rate;
            b.last = now;
        }
        else
        {
            double elapsedSec = std::chrono::duration<double>(now - b.last).count();
            b.tokens += elapsedSec * (double)m_rate;
            if (b.tokens > (double)m_rate)
            {
                b.tokens = (double)m_rate; // 桶容量 = rate
            }
            b.last = now;
        }

        if (b.tokens >= 1.0)
        {
            b.tokens -= 1.0;
            return true;
        }
        return false;
    }

    // 断线时清理该连接的令牌桶
    void Reset(NetCID cid)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_buckets.erase(cid);
    }

private:
    struct Bucket
    {
        double tokens = 0.0;
        std::chrono::steady_clock::time_point last{};
    };

    std::map<NetCID, Bucket> m_buckets;
    int32 m_rate = 0;
    std::mutex m_mutex;
};
