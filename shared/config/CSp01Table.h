// =============================================================================
// CSp01Table.h — 小怪属性配置 bin 的强类型解码 (单张 sp01 表)
//
// 用法 (客户端/服务器框架在 Init 后注入核心):
//   CBinReader rd; rd.LoadFile("sp01.bin");
//   std::map<uint32_t, battlecore::Sp01Config> cfg;
//   CSp01Table(&rd).BuildConfigMap(difficulty, cfg);
//   core.SetSp01Config(cfg);
//
// 与 CConfigTable<T> 的区别: 小怪属性是「(classId, sp01Id) → 属性」的全局平表, 无关卡维度
// (enemyInfo.plist 全局一份), 本类只负责按行解码 + 难度系数增量 + 组合键打包。
//
// 键 = Sp01ConfigKey(classId, sp01Id) = (classId<<16)|sp01Id。enemyInfo.plist 的 N (sp01Id)
// 只在同 name 内唯一 (boy_511 与 girl_511 并存), 单用 sp01Id 会互相覆盖, 故拼 classId 高 16 位。
//
// 换算口径 (对齐旧客户端 darklayerba / Sp01::rebirth / GameLevelData.h):
//   maxHp      = hp + kSp01DiffHp[diff]                                  // 整数 HP (内核 ×100 进 centi)
//   walkSpeed  = FxFromFloat((moveSpeed + kSp01DiffSpeed[diff]) * 3.0f)  // px/帧 @20Hz (客户端 ×60/20=×3)
//   hurtToHero = FxFromFloat(hurtHPToHero + kSp01DiffHurt[diff])         // Q16.16 HP (可为小数 0.5)
// =============================================================================
#pragma once

#include <cstdint>
#include <map>

#include "CBinReader.h"
#include "CConfigTable.h"
#include "../battlecore/BattleEntity.h"

namespace rsconfig {

// 难度增量 (与客户端 GameLevelData.h 的 Sp01_HP_INCREASE/Sp01_HURTHERO/Sp01_MOVE_SPEED 逐位对齐;
// difficulty 0=普通 1=困难 2=地狱, 越界按 0 处理)。
static const int32_t kSp01DiffHp[3]    = { 0, 50, 100 };
static const int32_t kSp01DiffHurt[3]  = { 0, 5, 15 };
static const float   kSp01DiffSpeed[3] = { 0.0f, 0.1f, 0.1f };

// sp01 表行 (schema: id/classId/sp01Id/hp/moveSpeed/hurtHPToHero)
struct Sp01Info {
    uint32_t id           = 0;
    uint8_t  classId      = 0;
    int32_t  sp01Id       = 0;
    int32_t  hp           = 0;
    float    moveSpeed    = 0.0f;
    float    hurtHPToHero = 0.0f;

    bool Load(const CBinReader& /*rd*/, const uint8_t* row, uint32_t len) {
        RowCursor c(row, len);
        id           = c.U32();
        classId      = c.U8();
        sp01Id       = c.I32();
        hp           = c.I32();
        moveSpeed    = c.F32();
        hurtHPToHero = c.F32();
        return c.ok;
    }
};

// sp01 表解码 → Sp01Config
class CSp01Table {
public:
    CSp01Table() : m_reader(nullptr) {}
    explicit CSp01Table(const CBinReader* r) : m_reader(r) {}

    bool Ok() const { return m_reader && m_reader->Ok(); }

    // 取 (classId, sp01Id) 的属性 (难度 0 = 普通)
    bool GetSp01Config(uint8_t classId, int32_t sp01Id, battlecore::Sp01Config& out) const {
        return GetSp01Config(classId, sp01Id, 0, out);
    }

    // 取 (classId, sp01Id) 的属性, 应用 difficulty(0/1/2) 难度增量
    bool GetSp01Config(uint8_t classId, int32_t sp01Id, uint8_t difficulty,
                       battlecore::Sp01Config& out) const;

    // 全量打包成 SetSp01Config 需要的 map (键 = Sp01ConfigKey(classId, sp01Id)), 应用难度增量
    void BuildConfigMap(uint8_t difficulty, std::map<uint32_t, battlecore::Sp01Config>& out) const;

private:
    const CBinReader* m_reader;
};

inline bool CSp01Table::GetSp01Config(uint8_t classId, int32_t sp01Id, uint8_t difficulty,
                                      battlecore::Sp01Config& out) const {
    if (!Ok()) return false;
    uint8_t diff = difficulty < 3 ? difficulty : 0;

    CConfigTable<Sp01Info> st(m_reader, "sp01");
    if (!st.Ok()) return false;

    bool found = false;
    st.ForEachId([&](uint32_t id) {
        if (found) return;
        Sp01Info v;
        if (!st.Get(id, v)) return;
        if (v.classId != classId || v.sp01Id != sp01Id) return;
        out.maxHp      = v.hp + kSp01DiffHp[diff];
        out.walkSpeed  = battlecore::FxFromFloat((v.moveSpeed + kSp01DiffSpeed[diff]) * 3.0f);
        out.hurtToHero = battlecore::FxFromFloat(v.hurtHPToHero + (float)kSp01DiffHurt[diff]);
        found = true;
    });
    return found;
}

inline void CSp01Table::BuildConfigMap(uint8_t difficulty,
                                       std::map<uint32_t, battlecore::Sp01Config>& out) const {
    out.clear();
    if (!Ok()) return;
    uint8_t diff = difficulty < 3 ? difficulty : 0;

    CConfigTable<Sp01Info> st(m_reader, "sp01");
    if (!st.Ok()) return;

    st.ForEachId([&](uint32_t id) {
        Sp01Info v;
        if (!st.Get(id, v)) return;
        battlecore::Sp01Config c;
        c.maxHp      = v.hp + kSp01DiffHp[diff];
        c.walkSpeed  = battlecore::FxFromFloat((v.moveSpeed + kSp01DiffSpeed[diff]) * 3.0f);
        c.hurtToHero = battlecore::FxFromFloat(v.hurtHPToHero + (float)kSp01DiffHurt[diff]);
        out[battlecore::Sp01ConfigKey(v.classId, v.sp01Id)] = c;
    });
}

} // namespace rsconfig
