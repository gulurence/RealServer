// luacore_battle.i — 战斗模块编解码 + 战斗核心绑定
//   si_pb_battle_*    — 战斗协议编解码 (package PbBattle)
//   si_battlecore_*   — 共享战斗核心 (battlecore/CBattleCore)
// Lua: luacore_battle.si_pb_battle_* / luacore_battle.si_battlecore_*
%module luacore_battle

%include <typemaps.i>
%include <std_string.i>
%include <stdint.i>

%{
#include "xBase/xDefine.h"
#include "msg/msg_id.pb.h"
#include "msg/battle.pb.h"
#include "battlecore/BattleCore.h"
#include "battlecore/FixedPoint.h"
#include "config/LevelDataLoader.h"
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <cstdlib>
%}

%include "base/xDefine.i"
%include "pb_util.i"

// ================================================================
// 战斗模块编解码 — si_pb_battle_* (package PbBattle)
// ================================================================
%inline %{
// playersStr: "player_id,name,spawn_index,primary_gun,level,exp,gold,attack,defense,hp,g0,g1,...,g5;..."
// levelId/difficulty: 本场关卡/难度 (battle.lua 从 Redis battle:level 读出后回传, 客户端据此注入 bin)
std::string si_pb_battle_enter_rsp(int32 errorCode, int64 battleId, uint32 seed,
                                   uint32 maxFrame, const std::string& playersStr,
                                   uint32 levelId, uint32 difficulty) {
    PbBattle::PBBattleEnterRsp rsp;
    rsp.set_big_type(PbID::AT_BATTLE);
    rsp.set_small_type(PbBattle::BATTLE_ENTER_RSP);
    rsp.set_error_code((PbID::ErrorCode)errorCode);
    rsp.set_battle_id(battleId);
    rsp.set_seed(seed);
    rsp.set_max_frame(maxFrame);
    rsp.set_level_id(levelId);
    rsp.set_difficulty(difficulty);
    for (auto& rec : pb_split(playersStr, ';')) {
        if (rec.empty()) continue;
        auto f = pb_split(rec, ',');
        if (f.size() < 10) continue;
        auto* p = rsp.add_players();
        p->set_player_id(atoll(f[0].c_str()));
        p->set_name(f[1]);
        p->set_spawn_index((uint32)atoi(f[2].c_str()));
        p->set_primary_gun((uint32)atoi(f[3].c_str()));
        p->set_level((uint32)atoi(f[4].c_str()));
        p->set_exp((uint32)atoi(f[5].c_str()));
        p->set_gold((uint32)atoi(f[6].c_str()));
        p->set_attack((uint32)atoi(f[7].c_str()));
        p->set_defense((uint32)atoi(f[8].c_str()));
        p->set_hp((uint32)atoi(f[9].c_str()));
        // equip: f[10..] 为 6 槽载具 (原始 GunType), 拼回逗号串存储 (客户端按 str 读再切分)
        std::string equip;
        for (size_t i = 10; i < f.size(); ++i) {
            if (i > 10) equip += ",";
            equip += f[i];
        }
        p->set_equip(equip);
    }
    std::string out; rsp.SerializeToString(&out); return out;
}
std::string si_pb_battle_start_ntf(int64 battleId, uint32 seed) {
    PbBattle::PBBattleStartNtf ntf;
    ntf.set_big_type(PbID::AT_BATTLE);
    ntf.set_small_type(PbBattle::BATTLE_START_NTF);
    ntf.set_battle_id(battleId);
    ntf.set_seed(seed);
    std::string out; ntf.SerializeToString(&out); return out;
}
// inputsStr: "player_id,move_dir,shoot_dx,shoot_dy,skill_id,fire,gun_type;..."
std::string si_pb_battle_frame_ntf(uint32 frameId, const std::string& inputsStr) {
    PbBattle::PBBattleFrameNtf ntf;
    ntf.set_big_type(PbID::AT_BATTLE);
    ntf.set_small_type(PbBattle::BATTLE_FRAME_NTF);
    ntf.set_frame_id(frameId);
    for (auto& rec : pb_split(inputsStr, ';')) {
        if (rec.empty()) continue;
        auto f = pb_split(rec, ',');
        if (f.size() < 6) continue;
        auto* in = ntf.add_inputs();
        in->set_player_id(atoll(f[0].c_str()));
        in->set_move_dir((uint32)atoi(f[1].c_str()));
        in->set_shoot_dx((int32)atoi(f[2].c_str()));
        in->set_shoot_dy((int32)atoi(f[3].c_str()));
        in->set_skill_id((uint32)atoi(f[4].c_str()));
        in->set_fire(f[5] == "1");
        if (f.size() >= 7) in->set_gun_type((uint32)atoi(f[6].c_str()));
    }
    std::string out; ntf.SerializeToString(&out); return out;
}
std::string si_pb_battle_snapshot_ntf(uint32 frameId, const std::string& state) {
    PbBattle::PBBattleSnapshotNtf ntf;
    ntf.set_big_type(PbID::AT_BATTLE);
    ntf.set_small_type(PbBattle::BATTLE_SNAPSHOT_NTF);
    ntf.set_frame_id(frameId);
    ntf.set_state(state);
    std::string out; ntf.SerializeToString(&out); return out;
}
std::string si_pb_battle_end_ntf(int64 battleId, int32 winSide, const std::string& stats) {
    PbBattle::PBBattleEndNtf ntf;
    ntf.set_big_type(PbID::AT_BATTLE);
    ntf.set_small_type(PbBattle::BATTLE_END_NTF);
    ntf.set_battle_id(battleId);
    ntf.set_win_side(winSide);
    ntf.set_stats(stats);
    std::string out; ntf.SerializeToString(&out); return out;
}
std::string si_pb_battle_leave_rsp(int32 errorCode) {
    PbBattle::PBBattleLeaveRsp rsp;
    rsp.set_big_type(PbID::AT_BATTLE);
    rsp.set_small_type(PbBattle::BATTLE_LEAVE_RSP);
    rsp.set_error_code((PbID::ErrorCode)errorCode);
    std::string out; rsp.SerializeToString(&out); return out;
}
%}

// ================================================================
// 战斗核心绑定 — si_battlecore_* (Lua 侧持有 handle = 注册表 id)
//
// 说明: 不能把 CBattleCore* 直接当 int64 句柄传给 Lua —— 64 位地址经
// Lua double 往返会丢精度 (2^53 截断)。改用进程内注册表, Lua 侧只持有
// 小整数 id, 每次调用按 id 查回指针。
// ================================================================
%{
static std::map<int64, battlecore::CBattleCore*> g_battlecore_registry;
static int64 g_battlecore_next_id = 1;

static battlecore::CBattleCore* si_battlecore_get(int64 handle) {
    auto it = g_battlecore_registry.find(handle);
    return it == g_battlecore_registry.end() ? nullptr : it->second;
}

// 按 player_id 查英雄状态 (结算/展示用; 找不到返回 nullptr)。只读, 不影响确定性。
static const battlecore::HeroState* si_battlecore_find_hero(battlecore::CBattleCore* core, int64 playerId) {
    if (!core) return nullptr;
    for (const auto& h : core->GetHeroes()) {
        if (h.player_id == playerId) return &h;
    }
    return nullptr;
}
%}

%inline %{
int64 si_battlecore_new() {
    battlecore::CBattleCore* core = new battlecore::CBattleCore();
    int64 id = g_battlecore_next_id++;
    g_battlecore_registry[id] = core;
    return id;
}
// playerIdsStr: "player_id,player_id,..."
bool si_battlecore_init(int64 handle, uint32 seed, const std::string& playerIdsStr) {
    battlecore::CBattleCore* core = si_battlecore_get(handle);
    if (!core) return false;
    std::vector<int64_t> ids;
    for (auto& p : pb_split(playerIdsStr, ',')) {
        if (!p.empty()) ids.push_back((int64_t)atoll(p.c_str()));
    }
    return core->Init(seed, ids);
}
void si_battlecore_submit_input(int64 handle, int64 playerId, uint32 moveDir,
                                int32 shootDx, int32 shootDy, uint32 skillId, bool fire, uint32 gunType) {
    battlecore::CBattleCore* core = si_battlecore_get(handle);
    if (!core) return;
    battlecore::PlayerInput in;
    in.player_id = playerId;
    in.move_dir  = moveDir;
    in.shoot_dx  = shootDx;   // fx_t = int32_t (Q16.16 定点方向向量)
    in.shoot_dy  = shootDy;
    in.skill_id  = skillId;
    in.fire      = fire;
    in.gun_type  = gunType;
    core->SubmitInput(in);
}
// C6 S5: 从真实关卡 bin (shared/config/data/) 注入碰撞/波次/小怪/炮台。与客户端共用
// 同一份 rsconfig::LevelDataLoader → 两端核心初始状态逐位一致 (哈希可比)。
// levelId 由匹配流传入 (当前 battle.lua 固定 11); 必须在 si_battlecore_init 之后、
// 首次 si_battlecore_tick 之前调用。bin 目录相对服务器进程 cwd (= 仓库根)。
bool si_battlecore_setup_level(int64 handle, uint32 levelId, uint32 difficulty) {
    battlecore::CBattleCore* core = si_battlecore_get(handle);
    if (!core) return false;
    std::string err;
    return rsconfig::LevelDataLoader::InjectFromDir(*core, "shared/config/data", levelId, (uint8_t)difficulty, err);
}
// 设置英雄初始枪械 (局外系统传入的玩家主枪; 在 init + setup_level 之后、首次 tick 前调用)
bool si_battlecore_set_gun(int64 handle, int64 playerId, uint32 gunType) {
    battlecore::CBattleCore* core = si_battlecore_get(handle);
    return core && core->SetHeroGun(playerId, (uint8_t)gunType);
}
void si_battlecore_tick(int64 handle, uint32 frame) {
    battlecore::CBattleCore* core = si_battlecore_get(handle);
    if (!core) return;
    core->Tick(frame);
}
std::string si_battlecore_serialize(int64 handle) {
    battlecore::CBattleCore* core = si_battlecore_get(handle);
    return core ? core->SerializeState() : std::string();
}
// 返回 int64 而非 uint64: SWIG 对 uint64 走 lua_Number(double) 丢失 53 位精度,
// 且 >=2^63 时 Lua %d 无法格式化; 对 int64 走 lua_Integer 精确 64 位。这里把
// uint64 哈希按位重解释为 int64 (two's complement), 与客户端读回字段
// (si_pb_varint_field 返回 int64) 的符号表示一致, 保证 Lua 侧精确比较。
int64 si_battlecore_hash(int64 handle) {
    battlecore::CBattleCore* core = si_battlecore_get(handle);
    return core ? (int64)core->HashState() : 0;
}
int32 si_battlecore_check_win(int64 handle) {
    battlecore::CBattleCore* core = si_battlecore_get(handle);
    return core ? core->CheckWin() : 0;
}
int32 si_battlecore_get_hero_hp(int64 handle, int64 playerId) {
    const battlecore::HeroState* h = si_battlecore_find_hero(si_battlecore_get(handle), playerId);
    return h ? h->hp : -1;
}
int32 si_battlecore_get_hero_max_hp(int64 handle, int64 playerId) {
    const battlecore::HeroState* h = si_battlecore_find_hero(si_battlecore_get(handle), playerId);
    return h ? h->maxHp : -1;
}
int32 si_battlecore_get_hero_alive(int64 handle, int64 playerId) {
    const battlecore::HeroState* h = si_battlecore_find_hero(si_battlecore_get(handle), playerId);
    return h ? (h->alive ? 1 : 0) : -1;
}
int32 si_battlecore_get_hero_kills(int64 handle, int64 playerId) {
    const battlecore::HeroState* h = si_battlecore_find_hero(si_battlecore_get(handle), playerId);
    return h ? h->killCount : 0;
}
// 死亡/复活 (合作玩法): 单英雄累计死亡次数 / 复活倒计时帧 (只读, 不影响确定性)
int32 si_battlecore_get_hero_deaths(int64 handle, int64 playerId) {
    const battlecore::HeroState* h = si_battlecore_find_hero(si_battlecore_get(handle), playerId);
    return h ? h->deathCount : 0;
}
int32 si_battlecore_get_hero_respawn_timer(int64 handle, int64 playerId) {
    const battlecore::HeroState* h = si_battlecore_find_hero(si_battlecore_get(handle), playerId);
    return h ? h->respawnTimer : 0;
}
// 全员累计死亡总数 (req3: 总死亡次数 > 10 触发结算)
int32 si_battlecore_total_deaths(int64 handle) {
    battlecore::CBattleCore* core = si_battlecore_get(handle);
    if (!core) return 0;
    int32_t total = 0;
    for (const auto& h : core->GetHeroes()) total += h.deathCount;
    return total;
}
int32 si_battlecore_all_waves_cleared(int64 handle) {
    battlecore::CBattleCore* core = si_battlecore_get(handle);
    return core ? (core->GetWave().allWavesCleared ? 1 : 0) : 0;
}
// 监控上报: 生成本场战斗核心状态的单行 JSON (frame/hash/win/wave + 每个英雄的确定性状态),
// 供 battle.lua 经 si_actor_set_report_extra 缓存到 Actor, 再由心跳 Sweep 写监控 Redis 键。
std::string si_battlecore_report_json(int64 handle) {
    battlecore::CBattleCore* core = si_battlecore_get(handle);
    if (!core) return "{}";

    const battlecore::WaveState& w = core->GetWave();
    std::string out;
    out.reserve(512);
    out += "{\"frame\":" + std::to_string((int64)core->GetFrame());
    out += ",\"hash\":" + std::to_string((int64)core->HashState());
    out += ",\"win\":" + std::to_string((int64)core->CheckWin());
    out += ",\"wave\":" + std::to_string((int64)w.waveNum);
    out += ",\"wave_show\":" + std::to_string((int64)w.xianShiWaveNum);
    out += ",\"wave_timer\":" + std::to_string((int64)w.waveTimer);
    out += ",\"all_cleared\":" + std::string(w.allWavesCleared ? "true" : "false");

    out += ",\"heroes\":[";
    bool first = true;
    for (const auto& h : core->GetHeroes()) {
        if (!first) out += ",";
        first = false;
        out += "{\"id\":" + std::to_string((int64)h.player_id);
        out += ",\"eid\":" + std::to_string((int64)h.id);
        out += ",\"x\":" + std::to_string((int64)battlecore::FxToIntRound(h.x));
        out += ",\"y\":" + std::to_string((int64)battlecore::FxToIntRound(h.y));
        out += ",\"hp\":" + std::to_string((int64)h.hp);
        out += ",\"max_hp\":" + std::to_string((int64)h.maxHp);
        out += ",\"alive\":" + std::string(h.alive ? "true" : "false");
        out += ",\"gun\":" + std::to_string((int64)h.gunType);
        out += ",\"kills\":" + std::to_string((int64)h.killCount);
        out += ",\"bomb\":" + std::to_string((int64)h.bombFuse);
        out += ",\"move\":" + std::to_string((int64)h.moveDir);
        out += ",\"action\":" + std::to_string((int64)h.action);
        out += ",\"anim\":" + std::to_string((int64)h.animDir);
        out += ",\"last_shoot\":" + std::to_string((int64)h.lastShootFrame);
        out += "}";
    }
    out += "]";

    // 场景对象: 小怪/大怪 (Sp01State) — 坐标/生命/攻击(hurtToHero 对英雄单次伤害)/动作
    out += ",\"monsters\":[";
    bool mfirst = true;
    for (const auto& m : core->GetSp01s()) {
        if (m.remove) continue;   // 跳过待移除 (已死亡) 实体
        if (!mfirst) out += ",";
        mfirst = false;
        out += "{\"id\":" + std::to_string((int64)m.id);
        out += ",\"cls\":" + std::to_string((int64)m.classId);
        out += ",\"x\":" + std::to_string((int64)battlecore::FxToIntRound(m.x));
        out += ",\"y\":" + std::to_string((int64)battlecore::FxToIntRound(m.y));
        out += ",\"hp\":" + std::to_string((int64)m.hp);
        out += ",\"max_hp\":" + std::to_string((int64)m.maxHp);
        out += ",\"atk\":" + std::to_string((int64)battlecore::FxToIntRound(m.hurtToHero));
        out += ",\"action\":" + std::to_string((int64)m.action);
        out += "}";
    }
    out += "]";

    // 场景对象: 炮台 (TurretState)
    out += ",\"turrets\":[";
    bool tfirst = true;
    for (const auto& t : core->GetTurrets()) {
        if (t.remove) continue;
        if (!tfirst) out += ",";
        tfirst = false;
        out += "{\"id\":" + std::to_string((int64)t.id);
        out += ",\"x\":" + std::to_string((int64)battlecore::FxToIntRound(t.x));
        out += ",\"y\":" + std::to_string((int64)battlecore::FxToIntRound(t.y));
        out += ",\"hp\":" + std::to_string((int64)t.hp);
        out += ",\"max_hp\":" + std::to_string((int64)t.maxHp);
        out += ",\"atk\":" + std::to_string((int64)battlecore::FxToIntRound(t.hurtToHero));
        out += ",\"active\":" + std::string(t.active ? "true" : "false");
        out += "}";
    }
    out += "]";

    out += ",\"counts\":{\"hero\":" + std::to_string((int64)core->GetHeroes().size());
    out += ",\"sp01\":" + std::to_string((int64)core->GetSp01s().size());
    out += ",\"bullet\":" + std::to_string((int64)core->GetBullets().size());
    out += ",\"turret\":" + std::to_string((int64)core->GetTurrets().size());
    out += ",\"prop\":" + std::to_string((int64)core->GetProps().size());
    out += "}}";
    return out;
}
void si_battlecore_free(int64 handle) {
    auto it = g_battlecore_registry.find(handle);
    if (it == g_battlecore_registry.end()) return;
    delete it->second;
    g_battlecore_registry.erase(it);
}
%}
