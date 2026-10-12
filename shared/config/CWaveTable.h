// =============================================================================
// CWaveTable.h — 波次配置 bin 的强类型解码 (跨 wave / wave_spawn 两张表)
//
// 用法 (客户端/服务器框架在 Init 后注入核心):
//   CBinReader rd; rd.LoadFile("xxx.bin");
//   std::vector<battlecore::WaveDef> plan;
//   if (CWaveTable(&rd).GetWavePlan(levelId, plan)) core.SetWavePlan(plan);
//
// 与 CConfigTable<T> 的区别: 波次是「波 → 出生组」的两级嵌套, 分两张平表存
// (wave 表一行一波, wave_spawn 表一行一个出生组), 本类跨两表按 levelId 重组。
//
// levelId 约定 = big*10 + small (11..68), 与 out/out{bb}{ss}.plist、map/level{bb}{ss}.tmx 一致。
// 行 id 为导出时按 (levelId, waveNum, seq) 升序的自增序号, ForEachId 升序遍历即正确顺序。
// =============================================================================
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <vector>

#include "CBinReader.h"
#include "CConfigTable.h"
#include "../battlecore/BattleEntity.h"

namespace rsconfig {

// wave 表行 (schema: id/levelId/waveNum/isKey)
struct WaveInfo {
    uint32_t id      = 0;
    uint32_t levelId = 0;
    uint32_t waveNum = 0;
    bool     isKey   = false;

    bool Load(const CBinReader& /*rd*/, const uint8_t* row, uint32_t len) {
        RowCursor c(row, len);
        id      = c.U32();
        levelId = c.U32();
        waveNum = c.U32();
        isKey   = (c.U8() != 0);
        return c.ok;
    }
};

// wave_spawn 表行 (schema: id/levelId/waveNum/seq/classId/sp01Id/count/interval/entryPoint/startTime)
struct WaveSpawnInfo {
    uint32_t id         = 0;
    uint32_t levelId    = 0;
    uint32_t waveNum    = 0;
    uint32_t seq        = 0;
    uint8_t  classId    = 0;
    int32_t  sp01Id     = 0;
    int32_t  count      = 0;
    int32_t  interval   = 0;
    int32_t  entryPoint = 1;
    int32_t  startTime  = 0;

    bool Load(const CBinReader& /*rd*/, const uint8_t* row, uint32_t len) {
        RowCursor c(row, len);
        id         = c.U32();
        levelId    = c.U32();
        waveNum    = c.U32();
        seq        = c.U32();
        classId    = c.U8();
        sp01Id     = c.I32();
        count      = c.I32();
        interval   = c.I32();
        entryPoint = c.I32();
        startTime  = c.I32();
        return c.ok;
    }
};

// 跨两表解码 WavePlan
class CWaveTable {
public:
    CWaveTable() : m_reader(nullptr) {}
    explicit CWaveTable(const CBinReader* r) : m_reader(r) {}

    bool Ok() const { return m_reader && m_reader->Ok(); }

    // 取某关 (levelId = big*10+small) 的波次计划; 关卡不存在返回 false
    bool GetWavePlan(uint32_t levelId, std::vector<battlecore::WaveDef>& out) const;

private:
    const CBinReader* m_reader;
};

inline bool CWaveTable::GetWavePlan(uint32_t levelId, std::vector<battlecore::WaveDef>& out) const {
    out.clear();
    if (!Ok()) return false;

    CConfigTable<WaveInfo>     wt(m_reader, "wave");
    CConfigTable<WaveSpawnInfo> st(m_reader, "wave_spawn");
    if (!wt.Ok() || !st.Ok()) return false;

    // 1) 本关波数 + isKey
    std::map<uint32_t, bool> isKeyByWave;
    wt.ForEachId([&](uint32_t id) {
        WaveInfo w;
        if (wt.Get(id, w) && w.levelId == levelId) isKeyByWave[w.waveNum] = w.isKey;
    });
    if (isKeyByWave.empty()) return false;

    uint32_t maxWave = isKeyByWave.rbegin()->first;
    out.resize(maxWave);
    for (const auto& kv : isKeyByWave) {
        if (kv.first >= 1 && kv.first <= out.size()) out[kv.first - 1].isKey = kv.second;
    }

    // 2) 出生组 (ForEachId 升序 = 导出序 = levelId→waveNum→seq)
    st.ForEachId([&](uint32_t id) {
        WaveSpawnInfo s;
        if (!st.Get(id, s) || s.levelId != levelId) return;
        if (s.waveNum < 1 || s.waveNum > out.size()) return;
        battlecore::WaveSpawnGroup g;
        g.classId    = s.classId;
        g.sp01Id     = s.sp01Id;
        g.count      = s.count;
        g.interval   = s.interval;
        g.entryPoint = s.entryPoint;
        g.startTime  = s.startTime;
        out[s.waveNum - 1].groups.push_back(g);
    });
    return true;
}

} // namespace rsconfig
