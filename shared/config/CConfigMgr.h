/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: CConfigMgr.h
      Purpose:   多 bin 配置管理器

      CConfigMgr 把多份 bin 抽象为可原子替换、带版本、可逐表 diff 的整体
      CommitBlobs 校验版本/聚合哈希后提交并经 IConfigChangeSink 通知, AggregateHash 由表名字典序+每表哈希聚合
  *****************************************************************/

// =============================================================================
// CConfigMgr.h — 多 bin 配置管理器 (纯 C++, 无引擎/无网络/无第三方依赖)
//
// 职责: 把「一份完整游戏配置」抽象成一个可原子替换、带版本、可逐表 diff 的整体。
//   服务器与客户端共享这里的加载/版本/原子替换/变更 diff 逻辑; 差异只在「变更如何
//   送达」——服务器用 Redis pub/sub 发布 (engine 侧 CRedisConfigSink 实现 IConfigChangeSink),
//   客户端用连接动态拉取 (CClientConfigMgr, C4)。传输层通过 IConfigChangeSink 解耦。
//
// 语义约定:
//   - version: 发布方(CI/管理工具/服务器)给出的单调递增内容版本, 由调用方显式传入;
//     管理器不自行递增。相同版本+相同内容 = 幂等(不重复通知); 版本更小 = 拒绝(防降级);
//     版本相同但内容不同 = 拒绝(版本与内容不一致)。
//   - contentHash: 全部表的聚合哈希 = 表名字典序 + 每表 contentHash (FNV-1a), 不含文件
//     路径。它识别的是「已发布 bundle 的精确内容」——客户端下载的是服务器同一份 bin 字节,
//     故两端聚合哈希一致 (供快速判等 + 下载后校验)。注意: 每表 contentHash 覆盖 schema(含
//     字符串表偏移)+data+index 原始字节, 所以「同样逻辑内容但重新打包/换字符串表布局」会得到
//     不同哈希 (即重打包会触发一次热更, 属可接受冗余, 不是 bug)。
//   - 变更 diff: 逐表比较 per-table contentHash (CBinReader::TableContentHash), 新增/修改/
//     删除的表名都进 changedTables, 供热更逐表增量刷新 (C6)。
//   - 表名全局唯一: 跨 bin 重复视为配置错误 (拒绝加载), 保证 FindReader 唯一命中。
//   - 原子替换: 全部 bin 加载成功才提交; 任一失败保持旧状态 (天然回滚)。
// =============================================================================
#pragma once

#include "CBinReader.h"

#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace rsconfig {

// 一次配置变更的载荷 (传输层无关; Redis JSON / 客户端协议都据此编码)
struct ConfigChange {
    uint32_t                  version = 0;
    uint64_t                  contentHash = 0;
    std::vector<std::string>  changedTables;
};

// 变更接收端接口 (服务器 = Redis publish 的封装; 客户端 = 连接拉取的封装; 单测 = 内存 fake)
class IConfigChangeSink {
public:
    virtual ~IConfigChangeSink() {}
    virtual void OnConfigChanged(const ConfigChange& ch) = 0;
};

// 一份 bin 的原始字节 (内存加载; 客户端拉取路径用, 服务器文件加载也会先读成 blob)
struct BinBlob {
    std::string            name;    // 逻辑标识 (日志/报错用, 不入内容哈希)
    std::vector<uint8_t>   bytes;   // RSCF bin 原始字节
};

class CConfigMgr {
public:
    CConfigMgr() = default;

    // 从文件加载一份 bundle (每个 path 一个 bin 文件, 可含多表)。见 header 语义。
    bool Load(const std::vector<std::string>& paths, uint32_t version, std::string& err);
    // 从内存加载 (客户端拉取下载的字节)。name 仅用于报错定位。
    bool LoadBlobs(const std::vector<BinBlob>& blobs, uint32_t version, std::string& err);

    uint32_t Version() const { return m_version; }
    uint64_t ContentHash() const { return m_contentHash; }
    bool     Empty() const { return m_bins.empty(); }

    // 计算一组 blob 的聚合哈希 (不提交; 客户端下载后校验/幂等判等用)。
    // 任一 blob 不可加载时返回 0 (与任何真实 bundle 的哈希都不冲突, 空 bundle 哈希非 0)。
    static uint64_t AggregateHash(const std::vector<BinBlob>& blobs);

    // 含指定表的 bin reader; 表不存在返回 nullptr
    const CBinReader* FindReader(const char* tableName) const;
    // 升序遍历某表所有 id (表名唯一, 直接命中对应 bin)
    void ForEachId(const char* tableName, const std::function<void(uint32_t)>& fn) const;

    void SetSink(IConfigChangeSink* sink) { m_sink = sink; }

private:
    // 核心: 暂存加载 → 内容哈希/版本校验 → 逐表 diff → 原子提交 → 通知
    bool CommitBlobs(const std::vector<BinBlob>& blobs, uint32_t version, std::string& err);
    // 由 per-table 哈希表算聚合内容哈希 (纯内容)
    static uint64_t AggregateFromHashes(const std::map<std::string, uint64_t>& tableHash);

    std::vector<CBinReader>         m_bins;      // 已提交的 bin (持有数据副本)
    std::map<std::string, uint64_t> m_tableHash; // tableName → 提交时的 per-table 内容哈希
    uint32_t        m_version = 0;
    uint64_t        m_contentHash = 0;
    IConfigChangeSink* m_sink = nullptr;
};

// ---------------------------------------------------------------------------

inline bool CConfigMgr::Load(const std::vector<std::string>& paths, uint32_t version, std::string& err) {
    err.clear();
    std::vector<BinBlob> blobs;
    blobs.reserve(paths.size());
    for (const std::string& p : paths) {
        BinBlob b; b.name = p;
        std::FILE* f = std::fopen(p.c_str(), "rb");
        if (!f) { err = "打开失败 [" + p + "]"; return false; }
        std::fseek(f, 0, SEEK_END);
        long n = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        b.bytes.resize(n > 0 ? (size_t)n : 0);
        if (n > 0) std::fread(b.bytes.data(), 1, (size_t)n, f);
        std::fclose(f);
        blobs.push_back(std::move(b));
    }
    return CommitBlobs(blobs, version, err);
}

inline bool CConfigMgr::LoadBlobs(const std::vector<BinBlob>& blobs, uint32_t version, std::string& err) {
    err.clear();
    return CommitBlobs(blobs, version, err);
}

inline bool CConfigMgr::CommitBlobs(const std::vector<BinBlob>& blobs, uint32_t version, std::string& err) {
    // 暂存到本地, 全部成功才提交 (原子替换; 失败时旧状态原封不动)
    std::vector<CBinReader> staged;
    std::map<std::string, uint64_t> stagedHash;
    staged.reserve(blobs.size());
    for (const BinBlob& b : blobs) {
        CBinReader r;
        if (!r.Load(b.bytes.data(), b.bytes.size())) { err = "加载失败 [" + b.name + "]: " + r.Error(); return false; }
        for (uint32_t i = 0; i < r.TableCount(); ++i) {
            const char* tn = r.TableName(i);
            if (!tn) continue;
            if (stagedHash.count(tn)) { err = "表名重复 [" + std::string(tn) + "] (跨 bin)"; return false; }
            stagedHash[tn] = r.TableContentHash(i);
        }
        staged.push_back(std::move(r));
    }

    // 聚合哈希: 表名字典序 + 每表 contentHash (不含路径)
    uint64_t agg = AggregateFromHashes(stagedHash);

    // 版本/内容一致性校验
    if (!m_bins.empty() && version <= m_version) {
        if (version == m_version && agg == m_contentHash) return true;   // 幂等: 相同版本相同内容
        err = "版本未递增 (当前 " + std::to_string(m_version) + ", 请求 " + std::to_string(version) + ")";
        return false;
    }

    // 逐表 diff: 新增/修改/删除都进 changedTables
    std::vector<std::string> changed;
    for (const auto& kv : stagedHash) {
        auto it = m_tableHash.find(kv.first);
        if (it == m_tableHash.end() || it->second != kv.second) changed.push_back(kv.first);
    }
    for (const auto& kv : m_tableHash) {
        if (stagedHash.find(kv.first) == stagedHash.end()) changed.push_back(kv.first);
    }

    // 提交
    m_bins = std::move(staged);
    m_tableHash = std::move(stagedHash);
    m_version = version;
    m_contentHash = agg;

    if (m_sink && !changed.empty()) {
        ConfigChange ch;
        ch.version = m_version;
        ch.contentHash = m_contentHash;
        ch.changedTables = std::move(changed);
        m_sink->OnConfigChanged(ch);
    }
    return true;
}

inline uint64_t CConfigMgr::AggregateFromHashes(const std::map<std::string, uint64_t>& tableHash) {
    uint64_t agg = Fnv1aInit();
    for (const auto& kv : tableHash) {
        for (const char* q = kv.first.c_str(); *q; ++q) Fnv1aStep(agg, (uint8_t)*q);
        Fnv1aStep(agg, 0);
        Fnv1aStepU64(agg, kv.second);
    }
    return agg;
}

inline uint64_t CConfigMgr::AggregateHash(const std::vector<BinBlob>& blobs) {
    std::map<std::string, uint64_t> h;
    for (const BinBlob& b : blobs) {
        CBinReader r;
        if (!r.Load(b.bytes.data(), b.bytes.size())) return 0;  // 任一 bin 不可载 → 哈希无意义
        for (uint32_t i = 0; i < r.TableCount(); ++i) {
            const char* tn = r.TableName(i);
            if (tn) h[tn] = r.TableContentHash(i);
        }
    }
    return AggregateFromHashes(h);
}

inline const CBinReader* CConfigMgr::FindReader(const char* tableName) const {
    for (const auto& r : m_bins)
        if (r.TableIndex(tableName) >= 0) return &r;
    return nullptr;
}

inline void CConfigMgr::ForEachId(const char* tableName, const std::function<void(uint32_t)>& fn) const {
    const CBinReader* r = FindReader(tableName);
    if (r) r->ForEachId(tableName, fn);
}

} // namespace rsconfig
