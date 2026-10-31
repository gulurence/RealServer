/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: CActorReportMgr.h
      Purpose:   Actor 监控上报模块声明

      与 WebPlatform Actor 信息显示界面配合: 收到订阅请求持续上报, 超时 20s 停止
      声明 Init/OnReportRequest/Sweep, subscriber 线程与心跳线程经 m_lock 串行化
  *****************************************************************/

#pragma once

#include "xActor/ActorDefine.h"
#include "xBase/xSingleton.h"

#include <map>
#include <mutex>
#include <string>
#include <utility>

class CRedisCli;

// =============================================================================
// CActorReportMgr — Actor 数据监控上报模块
//
// 与 WebPlatform「Actor 信息显示界面」配合:
//   1. WebPlatform 每 5s 发布一次订阅请求 (channel=cluster:actor:report:req, JSON:
//      {"type":N,"id":M}), 表示"该 Actor 详情页仍打开, 请持续上报"。
//   2. 本模块订阅该频道, 收到请求即记录 ActorID 并刷新上报标记 (m_monitors 的最近订阅时间)。
//   3. 心跳线程周期调用 Sweep(): 对仍在有效期内 (≤20s) 的 Actor, 用其上报结构 BuildReportJson
//      写 Redis 键 cluster:actor:report:{type}:{id} (JSON String, TTL 15s 自动"重置")。
//   4. 若某 Actor 连续 20s 未再收到订阅 (页面关闭), Sweep 移除其标记, 停止上报。
//
// 线程模型: subscriber 线程写 m_monitors, 心跳线程读/清 m_monitors, 均经 m_lock 串行化。
// =============================================================================
class CActorReportMgr : public xSingleton<CActorReportMgr>
{
public:
    CActorReportMgr() = default;
    ~CActorReportMgr() = default;

    // 绑定共享 RedisCluster (CGmCommandMgr::GetCli) 并订阅请求频道
    bool Init(CRedisCli *cli);

    // subscriber 线程回调: 解析 {"type":N,"id":M}, 刷新上报标记
    void OnReportRequest(const std::string &msg);

    // 心跳线程周期调用: 上报存活 Actor / 清理超时标记
    void Sweep();

private:
    CRedisCli *m_cli = nullptr;
    // (type, 业务id) → 最近一次订阅请求时间 (Unix 秒)
    std::map<std::pair<EActorType, ActorID>, int64> m_monitors;
    std::mutex m_lock;
};
