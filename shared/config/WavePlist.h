/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: WavePlist.h
      Purpose:   波次 plist 离线解析为 WaveDef

      ParsePlist 解析最小 plist XML 值树, ParseMonsterId 按子串顺序映射怪物串为 classId
      PlistToWavePlan/WavePlanToTables 把 out 波次时间线转为 wave/wave_spawn 表, 秒→帧、60fps delay→20Hz interval
  *****************************************************************/

// =============================================================================
// WavePlist.h — Cocos plist(XML) → 波次编排 (WaveDef) 的离线解析与映射 (纯 C++)
//
// 用途: S1 数据授权。把 `out/out{bb}{ss}.plist` (关卡波次时间线) 离线解析成核心
// 的 `battlecore::WaveDef/WaveSpawnGroup` 序列, 再经 WavePlanToTables 转成
// `wave` / `wave_spawn` 两张 TableData 供 BuildBin 打包。
//
// 语义对齐旧客户端 (darklayerba / EBasa::createSp01 / Sp01Boss::createASp01):
//   - 顶层 waveNum = 波数; outNum = 每个 timeline 里最大 out 序号 (出生点序号上限)。
//   - wave{w}.timelines[]: timeline 在波开始后 startTime(秒) 触发, 内含若干 out{i}。
//   - out{i} = { delay(60fps 帧计数, 相邻怪 appear 间隔), duilie(有序怪物串数组) }。
//   - out 序号 i → entryPoint = i (getm_pEntrys[i-1]); 第 j 个怪 delay*j 帧后 appear。
//   - 怪物串 name: 前缀 → classId (子串匹配, 顺序敏感); "_" 后缀 → Sp01Info 行号 sp01Id。
//   - pt_N 非刷怪, 转炮台激活组 (classId=kClassTurret 哨兵 + sp01Id=N); drop_/dialog_ 跳过 (后续处理)。
//
// 转换口径 (记录于 docs/c6-level-data-authoring.md, P5 parity 时校准):
//   startTime(秒) → 帧 = round(×20); delay(60fps 帧) → interval(20Hz 帧) = (delay+1)/3。
//   异质 duilie 按「连续相同 (classId,sp01Id) run」拆组, run 首元素相对时间 = round(j*delay/3)。
//
// 纯 C++、无引擎依赖, 仅被导出工具与单测引用 (不入运行时内核路径)。
// =============================================================================
#pragma once

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include "../battlecore/BattleEntity.h"
#include "ConfigFormat.h"

namespace rsconfig {

// ---------------------------------------------------------------------------
// 最小 plist(XML) 值树: 只支持 <plist>/<dict>/<array>/<key>/<integer>/<string>/
// <real>/<true/>/<false/> (本项目的 out plist 只用到这些)。
// ---------------------------------------------------------------------------
struct PValue {
    enum Type { kInt, kReal, kStr, kBool, kDict, kArray } type = kInt;
    int64_t  i = 0;
    double   r = 0.0;
    bool     b = false;
    std::string s;
    std::vector<std::pair<std::string, PValue>> dict;  // 保序
    std::vector<PValue> arr;

    bool IsDict()  const { return type == kDict; }
    bool IsArray() const { return type == kArray; }
    const PValue* Find(const char* key) const {
        for (const auto& kv : dict) if (kv.first == key) return &kv.second;
        return nullptr;
    }
};

// 文本值 → double (兼容 <string>"40" / <real> / <integer>)
inline double PValueToDouble(const PValue& v) {
    if (v.type == PValue::kStr)  return std::strtod(v.s.c_str(), nullptr);
    if (v.type == PValue::kReal) return v.r;
    if (v.type == PValue::kInt)  return (double)v.i;
    return 0.0;
}

namespace plist_detail {

inline bool IsSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

// 读下一个标签 (跳过 <?...?>/<!...> 声明与注释)。
// name 输出标签名 (关闭标签带前导 '/'), selfClose 表示 '/>'。
inline bool ReadTag(const std::string& s, size_t& i, std::string& name, bool& selfClose) {
    for (;;) {
        size_t lt = s.find('<', i);
        if (lt == std::string::npos) return false;
        i = lt;
        if (i + 1 < s.size() && (s[i + 1] == '?' || s[i + 1] == '!')) {
            size_t gt = s.find('>', i);
            if (gt == std::string::npos) return false;
            i = gt + 1;
            continue;
        }
        break;
    }
    size_t j = i + 1;
    bool closing = false;
    selfClose = false;
    if (j < s.size() && s[j] == '/') { closing = true; ++j; }
    size_t ns = j;
    while (j < s.size() && s[j] != '>' && s[j] != '/' && !IsSpace(s[j])) ++j;
    if (ns == j) return false;
    name = (closing ? "/" : "") + s.substr(ns, j - ns);
    while (j < s.size() && s[j] != '>') { if (s[j] == '/') selfClose = true; ++j; }
    if (j >= s.size()) return false;
    i = j + 1;
    return true;
}

// 读 <name>text</name> 的文本 (i 已指向 <name> 之后)
inline bool ReadText(const std::string& s, size_t& i, const std::string& name, std::string& text) {
    size_t e = s.find("</" + name + ">", i);
    if (e == std::string::npos) return false;
    text = s.substr(i, e - i);
    i = e + name.size() + 3;
    return true;
}

inline bool ParseValue(const std::string& s, size_t& i, PValue& out, std::string& err);

inline bool ParseValue(const std::string& s, size_t& i, PValue& out, std::string& err) {
    std::string name;
    bool selfClose = false;
    if (!ReadTag(s, i, name, selfClose)) { err = "解析标签失败"; return false; }
    if (!name.empty() && name[0] == '/') { err = "意外关闭标签 " + name; return false; }

    if (name == "dict") {
        out.type = PValue::kDict;
        for (;;) {
            std::string n; bool sc = false;
            if (!ReadTag(s, i, n, sc)) { err = "dict: 提前结束"; return false; }
            if (n == "/dict") return true;
            if (sc) continue;
            if (n != "key") { err = "dict: 期望 <key> 实得 <" + n + ">"; return false; }
            std::string key;
            if (!ReadText(s, i, "key", key)) { err = "dict: 读 key 失败"; return false; }
            PValue val;
            if (!ParseValue(s, i, val, err)) return false;
            out.dict.push_back(std::make_pair(key, std::move(val)));
        }
    }
    if (name == "array") {
        out.type = PValue::kArray;
        for (;;) {
            size_t save = i;
            std::string n; bool sc = false;
            if (!ReadTag(s, i, n, sc)) { err = "array: 提前结束"; return false; }
            if (n == "/array") return true;
            if (sc) continue;
            i = save;   // 回退, 让 ParseValue 重新消费该元素
            PValue val;
            if (!ParseValue(s, i, val, err)) return false;
            out.arr.push_back(std::move(val));
        }
    }
    if (name == "string")  { out.type = PValue::kStr; return ReadText(s, i, "string", out.s); }
    if (name == "key")     { out.type = PValue::kStr; return ReadText(s, i, "key", out.s); }
    if (name == "integer") { std::string t; if (!ReadText(s, i, "integer", t)) return false;
                             out.type = PValue::kInt; out.i = std::strtoll(t.c_str(), nullptr, 10); return true; }
    if (name == "real")    { std::string t; if (!ReadText(s, i, "real", t)) return false;
                             out.type = PValue::kReal; out.r = std::strtod(t.c_str(), nullptr); return true; }
    if (name == "true")    { out.type = PValue::kBool; out.b = true;  return true; }
    if (name == "false")   { out.type = PValue::kBool; out.b = false; return true; }
    if (name == "data")    { size_t e = s.find("</data>", i); if (e == std::string::npos) return false;
                             i = e + 7; return true; }

    err = "未识别的标签 <" + name + ">";
    return false;
}

} // namespace plist_detail

// 整棵 plist XML → PValue 树 (跳过 <?xml?>/<!DOCTYPE> 声明与 <plist> 包装)
inline bool ParsePlist(const std::string& xml, PValue& out, std::string& err) {
    using namespace plist_detail;
    size_t i = 0;
    std::string name;
    bool sc = false;
    if (!ReadTag(xml, i, name, sc)) { err = "ParsePlist: 无标签"; return false; }
    if (name != "plist") { err = "ParsePlist: 根非 <plist> 而是 <" + name + ">"; return false; }
    if (!ParseValue(xml, i, out, err)) return false;
    if (!out.IsDict()) { err = "ParsePlist: 根不是 <dict>"; return false; }
    return true;
}

// ---------------------------------------------------------------------------
// 怪物串 → (classId, sp01Id); 特殊 token 返回 special=true
// 子串匹配顺序与 Sp01Boss::createASp01 严格一致 (boy_max 先于 boy 等)。
// ---------------------------------------------------------------------------
struct MonsterRef {
    uint8_t  classId = 0;
    int32_t  sp01Id  = 0;
    bool     special = false;
};

inline MonsterRef ParseMonsterId(const std::string& id) {
    MonsterRef r;
    auto has = [&](const char* k) { return id.find(k) != std::string::npos; };

    if      (has("boy_max"))    r.classId = battlecore::kClassSp01BoyMax;
    else if (has("girl"))       r.classId = battlecore::kClassSp01Girl;
    else if (has("dog"))        r.classId = battlecore::kClassSp01Dog;
    else if (has("chef"))       r.classId = battlecore::kClassSp01Chef;
    else if (has("dama"))       r.classId = battlecore::kClassSp01Dama;
    else if (has("boy"))        r.classId = battlecore::kClassSp01Boy;
    else if (has("police_max")) r.classId = battlecore::kClassSp01PoliceMax;
    else if (has("police"))     r.classId = battlecore::kClassSp01Police;
    else if (has("worker_max")) r.classId = battlecore::kClassSp01WorkerMax;
    else if (has("zombie_max")) r.classId = battlecore::kClassSp01ZombieMax;
    else if (has("zombie"))     r.classId = battlecore::kClassSp01Zombie;
    else if (has("worker"))     r.classId = battlecore::kClassSp01Worker;
    else if (has("boss01"))     r.classId = battlecore::kClassSp01BigGuard;
    else if (has("boss03"))     r.classId = battlecore::kClassSp01BigSPTeacher;
    else if (has("boss2"))      r.classId = battlecore::kClassSp01BigTeacher;
    else if (has("zhizhu"))     r.classId = battlecore::kClassSp01BigTeacherChild;
    else if (has("drop"))       { r.special = true; return r; }
    else if (has("pt_"))        { r.special = true; return r; }
    else if (has("dialog_"))    { r.special = true; return r; }
    else                        { r.special = true; return r; }  // 未知串: 确定性跳过

    // sp01Id = 最后一个 '_' 之后的数字 (name 可能自带 '_', 如 boy_max_53 → sp01Id=53, 而非首个
    // '_' 后的 "max_53"=0; 客户端按完整串 boy_max_53 查 enemyInfo, 与 rfind 语义一致)。
    size_t u = id.rfind('_');
    if (u != std::string::npos) r.sp01Id = (int32_t)std::strtol(id.c_str() + u + 1, nullptr, 10);
    return r;
}

// ---------------------------------------------------------------------------
// plist 根 (已 ParsePlist) → 波次计划
// ---------------------------------------------------------------------------
inline bool PlistToWavePlan(const PValue& root, uint32_t /*levelId*/,
                            std::vector<battlecore::WaveDef>& plan, std::string& err) {
    plan.clear();
    const PValue* waveNumV = root.Find("waveNum");
    const PValue* outNumV  = root.Find("outNum");
    if (!waveNumV || !outNumV) { err = "缺 waveNum/outNum"; return false; }
    int waveNum   = (int)waveNumV->i;
    int outNumAll = (int)outNumV->i;
    if (waveNum <= 0 || outNumAll <= 0) { err = "waveNum/outNum <= 0"; return false; }

    char key[32];
    for (int w = 1; w <= waveNum; ++w) {
        std::snprintf(key, sizeof(key), "wave%d", w);
        const PValue* wv = root.Find(key);
        if (!wv || !wv->IsDict()) { err = "缺 " + std::string(key); return false; }

        battlecore::WaveDef wd;
        wd.isKey = false;   // 48 关 plist 无 isKey 字段 → 恒 false (显示波次, 内核胜负不用)

        const PValue* timelines = wv->Find("timelines");
        if (timelines && timelines->IsArray()) {
            for (const PValue& tl : timelines->arr) {
                const PValue* st = tl.Find("startTime");
                double startSec = st ? PValueToDouble(*st) : 0.0;
                int32_t timelineStart = (int32_t)(startSec * 20.0 + 0.5);   // 秒 → 帧

                for (int o = 1; o <= outNumAll; ++o) {
                    std::snprintf(key, sizeof(key), "out%d", o);
                    const PValue* ov = tl.Find(key);
                    if (!ov || !ov->IsDict()) continue;

                    const PValue* dv = ov->Find("delay");
                    const PValue* duilie = ov->Find("duilie");
                    double delayFrames = dv ? PValueToDouble(*dv) : 0.0;    // 60fps 帧计数
                    int32_t interval = (int32_t)((delayFrames + 1.0) / 3.0); // → 20Hz 帧 (round-half-up)
                    if (!duilie || !duilie->IsArray()) continue;

                    int32_t n = (int32_t)duilie->arr.size();
                    int32_t j = 0;
                    while (j < n) {
                        const PValue& el = duilie->arr[j];
                        if (el.type != PValue::kStr) { ++j; continue; }

                        // 炮台激活 (pt_N): 复用 WaveSpawnGroup 的 kClassTurret 哨兵。
                        // 客户端 activeThisTurret(delayDelta*j) → 激活帧 = timelineStart + round(j*delay/3),
                        // appear 延迟 = 同一 round(j*delay/3) (60fps→20Hz); count=1 使事件恰触发一次。
                        if (el.s.compare(0, 3, "pt_") == 0) {
                            int32_t nameIndex = (int32_t)std::strtol(el.s.c_str() + 3, nullptr, 10);
                            int32_t rel = (int32_t)((delayFrames * (double)j + 1.0) / 3.0);
                            battlecore::WaveSpawnGroup g;
                            g.classId    = battlecore::kClassTurret;
                            g.sp01Id     = nameIndex;   // 炮台名序号 N
                            g.count      = 1;
                            g.interval   = rel;         // 复用: appear 延迟 (20Hz 帧)
                            g.entryPoint = o;
                            g.startTime  = timelineStart + rel;
                            wd.groups.push_back(g);
                            ++j;
                            continue;
                        }

                        MonsterRef m = ParseMonsterId(el.s);
                        if (m.special) { ++j; continue; }   // drop_/dialog_/未知 跳过

                        int32_t k = j + 1;   // 连续同 (classId,sp01Id) run 的右开界
                        while (k < n) {
                            if (duilie->arr[k].type != PValue::kStr) break;
                            MonsterRef m2 = ParseMonsterId(duilie->arr[k].s);
                            if (m2.special || m2.classId != m.classId || m2.sp01Id != m.sp01Id) break;
                            ++k;
                        }

                        battlecore::WaveSpawnGroup g;
                        g.classId    = m.classId;
                        g.sp01Id     = m.sp01Id;
                        g.count      = k - j;
                        g.interval   = interval;
                        g.entryPoint = o;
                        g.startTime  = timelineStart + (int32_t)((delayFrames * (double)j + 1.0) / 3.0);
                        wd.groups.push_back(g);
                        j = k;
                    }
                }
            }
        }
        plan.push_back(std::move(wd));
    }
    return true;
}

// ---------------------------------------------------------------------------
// 波次计划 → wave / wave_spawn 两张 TableData (行 id 自增, ForEachId 升序 = level→wave→seq)
// waveIdBase/spawnIdBase: 多关卡合成一张大表时用, 传入全局自增起点避免 id 冲突。
// ---------------------------------------------------------------------------
inline void WavePlanToTables(uint32_t levelId, const std::vector<battlecore::WaveDef>& plan,
                             TableData& waveTable, TableData& waveSpawnTable,
                             uint32_t waveIdBase = 1, uint32_t spawnIdBase = 1) {
    auto I32 = [](int64_t v) { Cell c; c.type = kTypeI32; c.i = v; return c; };
    auto U8  = [](int64_t v) { Cell c; c.type = kTypeU8;  c.i = v; return c; };
    auto B   = [](bool v)    { Cell c; c.type = kTypeBool; c.i = v ? 1 : 0; return c; };

    waveTable = TableData();
    waveTable.name = "wave";
    waveTable.fields = {
        FieldDesc("id",      kTypeI32),
        FieldDesc("levelId", kTypeI32),
        FieldDesc("waveNum", kTypeI32),
        FieldDesc("isKey",   kTypeBool),
    };

    waveSpawnTable = TableData();
    waveSpawnTable.name = "wave_spawn";
    waveSpawnTable.fields = {
        FieldDesc("id",         kTypeI32),
        FieldDesc("levelId",    kTypeI32),
        FieldDesc("waveNum",    kTypeI32),
        FieldDesc("seq",        kTypeI32),
        FieldDesc("classId",    kTypeU8),
        FieldDesc("sp01Id",     kTypeI32),
        FieldDesc("count",      kTypeI32),
        FieldDesc("interval",   kTypeI32),
        FieldDesc("entryPoint", kTypeI32),
        FieldDesc("startTime",  kTypeI32),
    };

    uint32_t wid = waveIdBase, sid = spawnIdBase;
    for (uint32_t w = 0; w < plan.size(); ++w) {
        const battlecore::WaveDef& wd = plan[w];
        Row wr;
        wr.id = wid++;
        wr.cells = { I32((int64_t)wr.id), I32((int64_t)levelId), I32((int64_t)(w + 1)), B(wd.isKey) };
        waveTable.rows.push_back(std::move(wr));

        for (uint32_t g = 0; g < wd.groups.size(); ++g) {
            const battlecore::WaveSpawnGroup& gg = wd.groups[g];
            Row sr;
            sr.id = sid++;
            sr.cells = {
                I32((int64_t)sr.id), I32((int64_t)levelId), I32((int64_t)(w + 1)), I32((int64_t)g),
                U8((int64_t)gg.classId), I32((int64_t)gg.sp01Id), I32((int64_t)gg.count),
                I32((int64_t)gg.interval), I32((int64_t)gg.entryPoint), I32((int64_t)gg.startTime),
            };
            waveSpawnTable.rows.push_back(std::move(sr));
        }
    }
}

} // namespace rsconfig
