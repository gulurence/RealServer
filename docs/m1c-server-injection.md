# M1-c 服务器注入碰撞 + 波次 + 配置 —— 实施计划

> 版本：v2（2026-09-15，已全部完成并冒烟验证）
> 目标：让服务器权威核心与客户端预测核心**注入同一份**地图碰撞 + 波次编排 + 小怪属性，
> 使两端 `corehash` 逐位一致，从而**解除 M3 暂缓的 HASH_NTF 上报**（客户端↔服务器哈希比对闭环）。

## 状态（2026-09-15 实测完成）

- ✅ **Stage A–D 完成（碰撞共享默认 + Lua 绑定 + 服务器注入 + 客户端切换单一真相）**：
  `shared/battlecore/GameData.h` 的 `MakeDefaultMapCollision()` 已与 TMX `level11.tmx` 逐字节一致
  （29 people + 35 bullet + 3 entry 矩形，值与顺序皆同，冒烟 corehash 切换前后逐位不变）；`luacore_battle.i`
  新增 `si_battlecore_setup_default`（一键注入默认三件套）；`battle.lua::ensure_core` 在 `si_battlecore_init`
  后调用之；`StageScene::exportCollisionToCore` 改 `col = MakeDefaultMapCollision()`。
- ✅ **Stage E 完成（解除 HASH_NTF 暂缓 + 逐位精确比对，冒烟验证）**：客户端 `UINetBattleLayer::sendHashNtf`
  每 50 帧上报 `battle_id/frame_id/state_hash`；服务器 `battle.lua` 的 `flush_frame` 把每帧
  `si_battlecore_hash` 归档进 `self.frameHash[frame]`，`handle_hash` 按 `frameId` 查档比对（不再用
  `self.frame == frameId` 门卫——那会导致服务器跑在客户端确认帧前面时永远不比对）。冒烟两端 frame=50/100/
  150/200 的 hash **逐位一致**，服务器日志 `[Battle] hash ok` 四条、无 `hash mismatch`。
- 🐛 **关键修复——SWIG int64 返回值精度丢失**：`handle_hash` 最初「服务器 hash ok 但值对不上」，
  服务器 log 里 hash=…1168 vs 客户端 …1400（差 232）。根因：SWIG 默认 `out` typemap 对 `long long`
  用 `lua_pushnumber`（double，≥2^53 会舍入，近 6.4e18 处 ULP≈1024）。已在 `base/xDefine.i` 加
  `%typemap(out) int64/uint64` 改用 `lua_pushinteger`（`lua_Integer` 64 位），逐位精确。**此 bug
  曾静默掩盖真实哈希失配（弱校验）**，修复后才是真正的逐位验证。
- ⏸️ **Stage F 炮台数据**：TMX 无 `paota_point` 对象层，炮台数据源待查，连同炮台渲染一并后续
  （不影响当前哈希——两端都无炮台）。

## 0. 背景与缺口（已核实）

- 服务器 `script/actors/battle/battle.lua` 的 `ensure_core` 只调 `si_battlecore_init(seed, roster)`，
  **未注入** `SetCollision / SetWavePlan / SetSp01Config`（`battle.lua:87-111`）。
- 客户端 `StageScene::initBattleCore` 已注入三样：`exportCollisionToCore()`（TMX 碰撞）+
  `MakeDefaultWavePlan()` + `MakeDefaultSp01Config()`（`shared/battlecore/GameData.h`）。
- 结果：服务器核心「空心」（无碰撞 → 英雄穿墙、无波次 → 不刷怪、无属性 → 怪无 HP/速度），
  客户端核心「满配」→ 两端哈希必然失配。M3 因此**只做客户端侧 `HashAtConfirmedFrame` 打点**，
  `VerifyFrame` + 客户端→服务器 `HASH_NTF` 上报暂缓。
- 波次 + 小怪属性已是**共享默认函数**（`GameData.h` 的 `MakeDefaultWavePlan/MakeDefaultSp01Config`），
  服务器直接调用即可（缺 Lua 绑定）。**唯一客户端独有**的是地图碰撞（TMX 派生）。

## 1. 关键事实

### 1.1 碰撞数据源 = TMX（`res` zip 内 `map/level11.tmx`）

- 联网地图固定 `level11.tmx`（`UINetBattleLayer.cpp:156`，`zip::map/level11.tmx`；`zip::` →
  可执行目录下 `res` zip，见 `CCFileUtils.cpp`）。
- 地图 `width=22 height=13 tilewidth=64 tileheight=64` → `contentSize=(1408, 832)` 像素。
- 对象层：`collide_people`（29 矩形）、`collide_bullet`（35 矩形）、`entry`（3 出生点）、
  `hero`（`out1` 出生）、`drop_point`；**无 `paota_point` 炮台对象层**（炮台数据见 §3 待办）。
- **坐标换算（cocos2d-x `CCTMXXMLParser.cpp:521-527`）**：TMX 用 Y 向下（Tiled），cocos 用 Y 向上，
  解析时 `cocosY = mapPixelHeight - rawY - height`（`832 - rawY - height`），`x = rawX`（无翻转）；
  这些 objectgroup 无 `x`/`y` 属性 → `positionOffset=0`。`entry` 的 y 在 `IOBuiledLay::addEntrys`
  里再 `+height`（`entryY = 832 - rawY`）。

### 1.2 注入 API 与绑定缺口

- `CBattleCore` 已有 `SetWavePlan / SetSp01Config / MutableCollision()`（`BattleCore.h:79-95`），
  但 **Lua 绑定缺失**：`luacore_battle.i` 目前只有 `new/init/submit/tick/serialize/hash/check_win/free`。
- `MapCollision{mapWidth,mapHeight, people[],bullet[],entries[]}`（`BattleEntity.h:399`），
  `WaveDef{WaveSpawnGroup{classId,sp01Id,count,interval,entryPoint,startTime}}`、`Sp01Config{maxHp,walkSpeed,hurtToHero}`。

### 1.3 哈希敏感性（待确认）

- `MapCollision` 是否进 `SerializeState/HashState` 待核实（`BattleCore.cpp`）。**若进**，则碰撞矩形
  **顺序**也必须两端一致（向量序列化顺序敏感）；**若不进**，则只需「值」一致（顺序只影响遍历、
  不影响 `ContainsPeople` 结果）。无论哪种，**值**必须逐位一致。

## 2. 方案（沿用 `GameData.h` 共享默认占位模式，C6 配置 bin 打通后替换）

> 与波次/小怪属性一致：把碰撞也做成 `GameData.h` 里的确定性共享默认 `MakeDefaultMapCollision()`，
> 客户端/服务器/回放调用同一函数 → 逐位一致。这是过渡占位，C6 配置 bin 接管后删。

### Stage A — 共享碰撞默认（本里程碑核心）

1. `shared/battlecore/GameData.h` 新增 `MakeDefaultMapCollision()`，返回 `level11.tmx` 的真实碰撞
   （按 §1.1 坐标换算 + TMX 文件序）。
2. **风险消解**：手写换算易与 cocos2d-x 实际输出有出入。稳妥做法 = **先 dump 后 author**：
   临时在 `StageScene::exportCollisionToCore` 后把 `MapCollision` 逐矩形打到 `sog_flow.log`，
   冒烟一次抓取真实值，据此 author `MakeDefaultMapCollision()`，再让客户端改用它并复跑冒烟，
   `corehash` 应与改前**逐位不变**（证明 author 数据 == TMX 派生数据）。

### Stage B — Lua 绑定

`engine/luacore/luacore_battle.i` 新增（字符串序列化注入，规避 SWIG 容器映射复杂度）：
- `si_battlecore_set_wave_plan(handle, planStr)`：`planStr` = 波次紧凑编码，解析成 `SetWavePlan`。
- `si_battlecore_set_sp01_config(handle, cfgStr)`：同上 → `SetSp01Config`。
- `si_battlecore_set_collision(handle, mapW, mapH, peopleStr, bulletStr, entryStr)`：同上 → `MutableCollision()`。

> 但更简单且零手工解析：直接在 `.i` 的 `%inline` 里 C++ 侧写 `si_battlecore_setup_default(handle)`
> （调用 `MakeDefaultWavePlan/MakeDefaultSp01Config/MakeDefaultMapCollision` 一键注入默认三件套），
> 服务器 `ensure_core` 只需一行。推荐此路径（配置 bin 打通前，默认三件套就是「唯一真相」）。

### Stage C — 服务器注入

`battle.lua::ensure_core` 在 `si_battlecore_init` 后调 `si_battlecore_setup_default(self.core)`；
`save_replay` 的校验核心 `vh` 同样注入（回放重放必须同配置，否则 verify 必失配）。

### Stage D — 客户端切换单一真相

`StageScene::exportCollisionToCore` 改为 `m_pred.MutableCore().MutableCollision() =
MakeDefaultMapCollision()`（替换 TMX 直拷）；`initBattleCore` 的 `SetWavePlan/SetSp01Config`
保持 `MakeDefaultWavePlan/MakeDefaultSp01Config` 不变。TMX 碰撞直拷路径删除（M4 单机也走核心时共用）。

### Stage E — 解除 HASH_NTF 暂缓

客户端 `drainServerFrames` 每 50 帧已有 `HashAtConfirmedFrame`；补一条客户端→服务器 `HASH_NTF`
上报（`UINetBattleLayer` 发送），服务器 `handle_hash` 已实现「失配→下发快照」闭环。M3 的
`onSnapshot → ResyncFromSnapshot` 已就绪，HASH_NTF 一接即通。

### Stage F — 炮台数据（后续）

TMX **无 `paota_point`** 对象层（炮台数据不在 `level11.tmx`，或由 `buildlayer*` 里带 `filename` 的
炮台图块表示，待查）。炮台渲染与服务器炮台注入**一并后续**（不影响当前哈希，因两端都无炮台）。

## 3. 待办问题（文档化）

1. **配置 bin 接管时机（C6）**：本里程碑用 `GameData.h` 共享默认（与波次/小怪一致）；完整走
   「TMX/plist → RSCFG bin → CConfigMgr → 两端注入」是 C6，需 author 碰撞 bin + 服务器加载 +
   客户端切换，暂缓。见 [[config-system-c1c2]]。
2. **碰撞是否入哈希 + 顺序敏感**：待读 `BattleCore.cpp` 的 `SerializeState/HashState` 确认；
   若入哈希，`MakeDefaultMapCollision` 的矩形顺序必须与客户端原 TMX 派生顺序一致（dump-author 已保证）。
3. **炮台数据源**：TMX 无 `paota_point`；炮台图块/位置从哪来待查（`Sp01Boss.cpp`/`Turret.cpp` 的
   `paota` 引用线索）。炮台渲染 + 服务器炮台注入 = 后续里程碑。
4. **服务器重建/重启**：本里程碑改 `.i`（SWIG）+ `battle.lua` → 需重建 `RealServer.exe` + 重启
   4 进程（gate/login/player/world），冒烟前必须完成；老进程不识别新绑定会报错。

## 4. 验证（冒烟）

1. 客户端编译绿（改 `StageScene.cpp`/`GameData.h`）。
2. 服务器重建 + 重启（新 `si_battlecore_setup_default` 绑定生效）。
3. `smoke_test.ps1`：两端 frame≥200；**新增判据**：服务器日志 `[Battle] hash` 无 mismatch（客户端
   HASH_NTF 与服务器 `si_battlecore_hash` 一致），客户端 `corehash` 打点两端仍逐位一致。
4. 无 `sog_crash.log`；回放校验 `match=true`（`save_replay` 的 verify 核心也注入默认三件套）。
