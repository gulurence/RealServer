/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: CClientConfigMgr.h
      Purpose:   客户端配置热更编排

      CClientConfigMgr 拉模型热更: LoadLocal 本地加载, OnNotify 按版本判断是否拉取
      ApplyPull/ReceiveBundle 经 IConfigTransport 拉字节→校验 contentHash→CConfigMgr 原子替换, 失败保持旧配置回滚
  *****************************************************************/

// =============================================================================
// CClientConfigMgr.h — 客户端配置热更编排 (纯 C++, 传输层解耦; C4 核心)
//
// 与服务器侧 (CConfigMgr + Redis 发布) 对称: 服务器把「新版本上线」这件事广播出去
// (version + contentHash), 客户端收到通知后**主动拉取** (pull 模型), 拉完校验哈希再
// 原子替换, 失败回滚到旧配置。设计文档 docs/config-system-design.md §C4。
//
// 职责边界:
//   - 本地初始加载: LoadLocal() 读打包进客户端的 bundle 文件。
//   - 收到服务器通知: OnNotify() 决定要不要拉 (版本更新才拉; 相同/更旧忽略; 防降级)。
//   - 拉取 + 校验 + 替换: ApplyPull() 通过 IConfigTransport 拉字节 → 校验 contentHash
//     → CConfigMgr::LoadBlobs 原子提交; 任一失败保持旧配置 (天然回滚)。
//   - 传输层 (真实 = 客户端连接层的 CONFIG_PULL_REQ/RSP 协议 + 分块下载, 随客户端工作流接;
//     单测 = 内存 fake) 只实现 IConfigTransport::Pull, 与本节拍逻辑完全无关。
// =============================================================================
#pragma once

#include "CConfigMgr.h"

#include <string>
#include <vector>

namespace rsconfig {

// 拉取传输抽象: 真实实现负责「分块下载 + 拼装 + 每个 bin 一份 blob」; 单测用内存 fake。
class IConfigTransport {
public:
    virtual ~IConfigTransport() {}
    // 拉取指定 version 的完整 bundle (每个 bin 文件 → 一个 BinBlob)。失败返回 false 并填 err。
    virtual bool Pull(uint32_t version, std::vector<BinBlob>& out, std::string& err) = 0;
};

class CClientConfigMgr {
public:
    // 本地初始加载 (打包进客户端的 bundle 文件)。version 由发布方随包提供。
    bool LoadLocal(const std::vector<std::string>& paths, uint32_t version, std::string& err) {
        return m_mgr.Load(paths, version, err);
    }

    // 收到服务器推送的通知 (version/contentHash)。返回是否需要拉取。
    //   版本更新 → 记录待拉取目标, 返回 true; 相同/更旧/更小 → false (忽略/防降级)。
    bool OnNotify(uint32_t version, uint64_t contentHash) {
        if (version <= m_mgr.Version()) return false;   // 相同或更旧: 不拉
        m_pendingVersion = version;
        m_pendingHash = contentHash;
        return true;
    }

    // 拉取 + 校验 + 原子替换。返回: true=已替换到新版本; false=失败, 保持旧配置 (回滚)。
    bool ApplyPull(IConfigTransport& transport, std::string& err) {
        err.clear();
        if (m_pendingVersion == 0) { err = "无待拉取版本 (未收到通知)"; return false; }

        std::vector<BinBlob> blobs;
        if (!transport.Pull(m_pendingVersion, blobs, err)) return false;   // 传输失败 → 保持旧配置

        // 下载后校验: 拉到的字节聚合哈希必须 == 服务器通知的 contentHash, 否则拒载 (不提交)
        uint64_t got = CConfigMgr::AggregateHash(blobs);
        if (got != m_pendingHash) {
            err = "内容哈希不符 (通知 " + std::to_string(m_pendingHash) +
                  " / 拉到 " + std::to_string(got) + "), 拒绝替换";
            return false;
        }

        // 原子替换: 全部 bin 可载才提交, 否则保持旧状态
        if (!m_mgr.LoadBlobs(blobs, m_pendingVersion, err)) return false;

        m_pendingVersion = 0;
        m_pendingHash = 0;
        return true;
    }

    // 收到服务器下发的完整 bundle (CONFIG_GET_RSP): 校验 contentHash → 原子替换。
    // 返回 true = 已应用 (或幂等); false = 哈希不符/加载失败, 保持旧配置 (天然回滚)。
    bool ReceiveBundle(uint32_t version, uint64_t contentHash,
                       const std::vector<BinBlob>& blobs, std::string& err) {
        err.clear();
        uint64_t got = CConfigMgr::AggregateHash(blobs);
        if (got == 0 || got != contentHash) {
            err = "内容哈希不符 (通知 " + std::to_string(contentHash) +
                  " / 拉到 " + std::to_string(got) + "), 拒绝替换";
            return false;
        }
        if (!m_mgr.LoadBlobs(blobs, version, err)) return false;
        m_pendingVersion = 0;
        m_pendingHash = 0;
        return true;
    }

    uint32_t Version() const { return m_mgr.Version(); }
    uint64_t ContentHash() const { return m_mgr.ContentHash(); }
    const CConfigMgr& Mgr() const { return m_mgr; }
    uint32_t PendingVersion() const { return m_pendingVersion; }

private:
    CConfigMgr m_mgr;
    uint32_t   m_pendingVersion = 0;   // 待拉取版本 (OnNotify 设置, ApplyPull 消费)
    uint64_t   m_pendingHash = 0;      // 待拉取内容哈希 (下载后校验用)
};

} // namespace rsconfig
