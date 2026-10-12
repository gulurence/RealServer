// luacore_spectate.i — 观战模块编解码绑定 (package PbSpectator)
// Lua: luacore_spectate.si_pb_spectate_*
%module luacore_spectate

%include <typemaps.i>
%include <std_string.i>
%include <stdint.i>

%{
#include "xBase/xDefine.h"
#include "msg/msg_id.pb.h"
#include "msg/spectator.pb.h"
#include "msg/battle.pb.h"
#include <string>
#include <vector>
#include <cstdint>
#include <cstdlib>
%}

%include "base/xDefine.i"
%include "pb_util.i"

// ================================================================
// 观战模块编解码 — si_pb_spectate_* (package PbSpectator)
// ================================================================
%inline %{
// playersStr: "player_id,name,spawn_index,primary_gun,level,exp,gold,attack,defense,hp,g0,g1,...,g5;..."
// 与 si_pb_battle_enter_rsp 逐字段同构, 客户端复用 PBBattleEnterRsp 的花名册解析进入战斗场景。
std::string si_pb_spectate_enter_rsp(int32 errorCode, int64 battleId, uint32 seed,
                                     const std::string& playersStr, uint32 levelId, uint32 difficulty) {
    PbSpectator::PBSpectateEnterRsp rsp;
    rsp.set_big_type(PbID::AT_SPECTATOR);
    rsp.set_small_type(PbSpectator::SPECTATE_ENTER_RSP);
    rsp.set_error_code((PbID::ErrorCode)errorCode);
    rsp.set_battle_id(battleId);
    rsp.set_seed(seed);
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
std::string si_pb_spectate_leave_rsp(int32 errorCode) {
    PbSpectator::PBSpectateLeaveRsp rsp;
    rsp.set_big_type(PbID::AT_SPECTATOR);
    rsp.set_small_type(PbSpectator::SPECTATE_LEAVE_RSP);
    rsp.set_error_code((PbID::ErrorCode)errorCode);
    std::string out; rsp.SerializeToString(&out); return out;
}
std::string si_pb_spectate_snapshot_ntf(uint32 frameId, const std::string& state) {
    PbSpectator::PBSpectateSnapshotNtf ntf;
    ntf.set_big_type(PbID::AT_SPECTATOR);
    ntf.set_small_type(PbSpectator::SPECTATE_SNAPSHOT_NTF);
    ntf.set_frame_id(frameId);
    ntf.set_state(state);
    std::string out; ntf.SerializeToString(&out); return out;
}
std::string si_pb_spectate_frame_ntf(uint32 frameId, const std::string& frame) {
    PbSpectator::PBSpectateFrameNtf ntf;
    ntf.set_big_type(PbID::AT_SPECTATOR);
    ntf.set_small_type(PbSpectator::SPECTATE_FRAME_NTF);
    ntf.set_frame_id(frameId);
    ntf.set_frame(frame);
    std::string out; ntf.SerializeToString(&out); return out;
}
std::string si_pb_spectate_end_ntf(int64 battleId) {
    PbSpectator::PBSpectateEndNtf ntf;
    ntf.set_big_type(PbID::AT_SPECTATOR);
    ntf.set_small_type(PbSpectator::SPECTATE_END_NTF);
    ntf.set_battle_id(battleId);
    std::string out; ntf.SerializeToString(&out); return out;
}
%}
