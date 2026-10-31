/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: luacore_config.i
      Purpose:   配置热更模块 Lua 绑定

      生成 %module luacore_config，提供 si_config_load 读取 wave/map_collision/sp01 三份 bin 到进程内全局 blob 缓存
      并导出 si_config_get_rsp / si_config_changed_ntf，contentHash 全程留在 C++ 侧编码进 proto，规避 64 位哈希经 Lua double 丢精度
  *****************************************************************/

// luacore_config.i — 配置热更模块编解码 + 配置加载 (package PbConfig)
//   si_config_load           — 读 3 份 bin → 全局 blob 缓存 + 聚合 contentHash
//   si_config_get_rsp        — 完整 bundle 响应 (version + contentHash + files)
//   si_config_changed_ntf    — 版本变更通知 (version + contentHash)
// Lua: luacore_config.si_config_*
//
// 说明: contentHash 是 uint64, 全程留在 C++ 侧 (g_config_hash) 编码进 proto,
// 不经过 Lua 的 double 中转, 规避 64 位哈希丢精度 (见 [[swig-int64-precision]])。
// Lua 只传 uint32 version。
%module luacore_config

%include <typemaps.i>
%include <std_string.i>
%include <stdint.i>

%{
#include "xBase/xDefine.h"
#include "msg/msg_id.pb.h"
#include "msg/config.pb.h"
#include "config/CConfigMgr.h"
#include <string>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <utility>
%}

%include "base/xDefine.i"

// ================================================================
// 配置加载 — C++ 全局 blob 缓存 (进程内单例, 由 Config Actor 持有)
// ================================================================
%{
static std::vector<rsconfig::BinBlob> g_config_blobs;
static uint64_t g_config_hash = 0;
%}

%inline %{
// 读 dir 下的 wave.bin / map_collision.bin / sp01.bin → 全局 blob 缓存 + 聚合
// contentHash (rsconfig::CConfigMgr::AggregateHash, 与客户端下载后校验同一份算法)。
// 任一 bin 缺失/不可载返回 false (清空缓存, 不部分提交)。
bool si_config_load(const std::string& dir) {
    g_config_blobs.clear();
    g_config_hash = 0;
    static const char* kNames[3] = { "wave.bin", "map_collision.bin", "sp01.bin" };
    for (int i = 0; i < 3; ++i) {
        std::string path = dir + "/" + kNames[i];
        std::FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) { g_config_blobs.clear(); g_config_hash = 0; return false; }
        std::fseek(f, 0, SEEK_END);
        long n = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        rsconfig::BinBlob b;
        b.name = kNames[i];
        b.bytes.resize(n > 0 ? (size_t)n : 0);
        if (n > 0) std::fread(b.bytes.data(), 1, (size_t)n, f);
        std::fclose(f);
        g_config_blobs.push_back(std::move(b));
    }
    g_config_hash = rsconfig::CConfigMgr::AggregateHash(g_config_blobs);
    return g_config_hash != 0;
}

// 完整 bundle 响应 (CONFIG_GET_RSP): version + contentHash + 三份 bin 字节。
std::string si_config_get_rsp(uint32_t version) {
    PbConfig::PBConfigGetRsp rsp;
    rsp.set_big_type(PbID::AT_CONFIG);
    rsp.set_small_type(PbConfig::CONFIG_GET_RSP);
    rsp.set_version(version);
    rsp.set_content_hash(g_config_hash);
    for (const auto& b : g_config_blobs) {
        auto* f = rsp.add_files();
        f->set_name(b.name);
        f->set_data(std::string(b.bytes.begin(), b.bytes.end()));
    }
    std::string out; rsp.SerializeToString(&out); return out;
}

// 版本变更通知 (CONFIG_CHANGED_NTF): version + contentHash, 客户端据版本比对重新拉取。
std::string si_config_changed_ntf(uint32_t version) {
    PbConfig::PBConfigChangedNtf ntf;
    ntf.set_big_type(PbID::AT_CONFIG);
    ntf.set_small_type(PbConfig::CONFIG_CHANGED_NTF);
    ntf.set_version(version);
    ntf.set_content_hash(g_config_hash);
    std::string out; ntf.SerializeToString(&out); return out;
}
%}
