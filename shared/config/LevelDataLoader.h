/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: LevelDataLoader.h
      Purpose:   关卡配置加载注入核心

      LevelDataLoader 解码 wave/map_collision/sp01 三份 bin 并原子注入 CBattleCore
      InjectFromDir 本地直读, InjectFromBlobs 客户端网络拉取, 共用 InjectFromReaders 纯解码无 I/O
  *****************************************************************/

// =============================================================================
// LevelDataLoader.h — C6 S5 收口: 从关卡 bin 目录加载并注入核心
//
// 客户端 (MSVC) 与服务器 (gcc) 共用同一份实现, 保证两端核心初始状态逐位一致
// (哈希可比)。把离线授权的三份 bin (wave/map_collision/sp01, 见
// shared/config/data/) 解码成强类型表, 再注入 CBattleCore:
//
//   碰撞  CMapTable(&map).GetMapCollision(levelId)            → SetCollision
//   波次  CWaveTable(&wave).GetWavePlan(levelId)              → SetWavePlan
//   小怪  CSp01Table(&sp01).BuildConfigMap(difficulty)        → SetSp01Config
//   炮台  CTurretTable(&sp01).BuildTurretConfigMap()          → SetTurretConfig
//         CTurretTable(&map).GetTurretSpawns(levelId)         → SetTurretSpawns
//
// C4 起提供两条数据源:
//   InjectFromDir   — 服务器/单机本地文件直读 (服务器权威, 单机回退)。
//   InjectFromBlobs — 客户端网络拉取的完整 bundle (CClientConfigMgr) 注入。
// 二者共用 InjectFromReaders (纯解码, 无 I/O)。
//
// 纯 C++ 头文件零依赖 (除 shared/config + battlecore), 不依赖 Cocos / 引擎。
// =============================================================================
#pragma once

#include "CBinReader.h"
#include "CConfigMgr.h"
#include "CWaveTable.h"
#include "CMapTable.h"
#include "CSp01Table.h"
#include "CTurretTable.h"
#include "../battlecore/BattleCore.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace rsconfig {

struct LevelDataLoader {
    // 从三个已加载的 reader 解码并注入 (纯解码, 无 I/O)。碰撞缺失即失败 (无碰撞
    // 无法战斗); 波次允许空 (无波次关则零怪, 不视为错误)。全部解码成功后才落 core
    // (原子注入: 先解到局部变量, 成功后再写 core)。
    static bool InjectFromReaders(battlecore::CBattleCore& core,
                                  CBinReader& wave, CBinReader& map, CBinReader& sp01,
                                  uint32_t levelId, uint8_t difficulty, std::string& err) {
        // 碰撞 (关卡缺失即失败: 无碰撞无法战斗)
        battlecore::MapCollision col;
        if (!CMapTable(&map).GetMapCollision(levelId, col)) { err = "关卡碰撞缺失: " + std::to_string(levelId); return false; }

        // 波次 (允许空: 无波次关则零怪, 不视为错误)
        std::vector<battlecore::WaveDef> plan;
        CWaveTable(&wave).GetWavePlan(levelId, plan);

        // 小怪属性 (全量配置 map, 按难度缩放)
        std::map<uint32_t, battlecore::Sp01Config> cfg;
        CSp01Table(&sp01).BuildConfigMap(difficulty, cfg);

        // 炮台属性 (sp01.bin 的 turret 表) + 炮台出生点 (map 的 turret_point 表)
        std::map<int32_t, battlecore::TurretConfig> tcfg;
        CTurretTable(&sp01).BuildTurretConfigMap(tcfg);
        std::vector<battlecore::TurretSpawn> spawns;
        CTurretTable(&map).GetTurretSpawns(levelId, spawns);

        // 全部解码成功后才落 core (原子注入)
        core.MutableCollision() = col;
        core.SetWavePlan(plan);
        core.SetSp01Config(cfg);
        core.SetTurretConfig(tcfg);
        core.SetTurretSpawns(spawns);
        return true;
    }

    // 从目录 dir 加载三份 bin (wave.bin / map_collision.bin / sp01.bin) 并按
    // levelId + difficulty 注入 core。全部成功返回 true, 任一环节失败写 err 并返回
    // false。
    static bool InjectFromDir(battlecore::CBattleCore& core, const std::string& dir,
                              uint32_t levelId, uint8_t difficulty, std::string& err) {
        CBinReader wave, map, sp01;
        if (!wave.LoadFile(dir + "/wave.bin"))          { err = "wave.bin 加载失败: " + dir; return false; }
        if (!map.LoadFile(dir + "/map_collision.bin"))  { err = "map_collision.bin 加载失败: " + dir; return false; }
        if (!sp01.LoadFile(dir + "/sp01.bin"))          { err = "sp01.bin 加载失败: " + dir; return false; }
        return InjectFromReaders(core, wave, map, sp01, levelId, difficulty, err);
    }

    // 从内存 blob 注入 (客户端网络拉取路径): 按 blob 名找到 wave/map/sp01 三个
    // reader 后走同一套解码。blob 来自 CClientConfigMgr 拉取的完整 bundle。
    static bool InjectFromBlobs(battlecore::CBattleCore& core,
                                const std::vector<BinBlob>& blobs,
                                uint32_t levelId, uint8_t difficulty, std::string& err) {
        const BinBlob* wave = nullptr;
        const BinBlob* map  = nullptr;
        const BinBlob* sp01 = nullptr;
        for (const BinBlob& b : blobs) {
            if      (b.name == "wave.bin")          wave = &b;
            else if (b.name == "map_collision.bin") map  = &b;
            else if (b.name == "sp01.bin")          sp01 = &b;
        }
        if (!wave || !map || !sp01) { err = "缺 bin (需 wave/map_collision/sp01)"; return false; }

        CBinReader wr, mr, sr;
        if (!wr.Load(wave->bytes.data(), wave->bytes.size())) { err = "wave.bin 解码失败"; return false; }
        if (!mr.Load(map->bytes.data(),  map->bytes.size()))  { err = "map_collision.bin 解码失败"; return false; }
        if (!sr.Load(sp01->bytes.data(), sp01->bytes.size())) { err = "sp01.bin 解码失败"; return false; }
        return InjectFromReaders(core, wr, mr, sr, levelId, difficulty, err);
    }
};

} // namespace rsconfig
