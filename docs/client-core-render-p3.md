# P3 客户端渲染/逻辑分离 —— 实施计划（StageScene 改跑共享核心）

> 版本：v1（2026-09-15）
> 目标：把 `StageScene` 从「Cocos 实体自驱（浮点）」改成「读 `CBattleCore`（定点核心）→ 渲染」，
> 使客户端全游戏模拟与服务器权威核心（`script/actors/battle/battle.lua`）**同一份代码**，根治
> 客户端↔服务器漂移；随后接线预测/回滚（F1/F2/F3，`CPredictiveSim`）。单机也切核心（一套逻辑）。

## 状态（2026-09-15 实测更新）

- ✅ **M1-a 完成（编译绿 + 冒烟验证）**：地图碰撞导出 + 影子核心（`m_core.Init` + `exportCollisionToCore` +
  `doNetSimFrame` 内锁步 `SubmitInput/Tick` + `corehash` 打点）。冒烟抓到并修掉「花名册顺序」bug
  （`initBattleCore` 改按 `spawn_index` 升序，而非「本端在前」），修后两端 `corehash` 逐位一致。
- ✅ **M1-b 完成（编译绿 + 冒烟验证）**：新增 `shared/battlecore/GameData.h` 占位波次+小怪属性，
  `initBattleCore` 注入 `SetWavePlan`(3 波) + `SetSp01Config`(16 个 classId)。冒烟 `corehash` 两端
  逐位一致且 `sp01=4`（怪已刷出，核心不再空心）。真实波次/小怪数据仍在 Cocos plist（`out/out*.plist`
  / `plist/enemyInfo.plist`），C6 配置 bin 打通前用共享头文件占位。
- ✅ **M1-c 前置：英雄开火→产子弹（2026-09-15 完成，冒烟验证）**：核心 `Tick` 开火从占位 hitscan
  （`HurtSp01ById`）改为产子弹实体——新增 `GunToBulletClass`（枪型→弹类：狙击/散弹/火箭筒/机枪/手枪）
  + `SpawnHeroBullet`（枪口 = 英雄坐标 + 射击方向单位向量×20，对齐客户端联网路径），`pendingBullets`
  声明上移、帧末统一挂载，`lastShootFrame` 做开火冷却（`kHeroFireIntervalFrames=6` 占位）。冒烟
  `corehash` frame=50/100/150/200 两端**逐位一致**且 **`bullets=2`**（此前恒 `0`）、`sp01=4` 不变
  （脚本输入没瞄向怪，200 帧内未命中，非 bug）；无 `sog_crash.log`。
- ✅ **M1-d 完成（2026-09-15 完成，编译绿 + 冒烟验证）**：渲染源切核心。`do_GameRunning` 加联网早退分支
  `renderCoreState()`（英雄+子弹读 `m_core.GetHeroes()/GetBullets()` 驱动渲染壳：`changeAction` +
  `setPosition(FxToFloat)` + `setRotation`，**停掉联网浮点自模拟**）；`doNetSimFrame` 删「写 move_dir/action
  到 Cocos 英雄」块，`sync frame=` 改读核心。冒烟 `sync frame=` 英雄坐标两端逐位一致（镜像）、`corehash`
  frame=50/100/150/200 两端**逐位一致**、`bullets=2`（>0 帧）、无 `sog_crash.log`；单机路径未动
  （`IsNetBattle() && m_coreReady` 早退保护）。
- ✅ **M2 完成（2026-09-15 完成，编译绿 + 冒烟验证）**：全实体渲染（小怪+道具）。`renderCoreSp01s` 复用
  `Sp01` 类作渲染壳（`setKeyFrameEventHandle(NULL)` 屏蔽攻击命中伤英雄/死亡掉道具/分裂出怪回调；不调度
  `update()` → 无 AI；`MigSpriteNode` 自 `scheduleUpdate()` 驱动动画）；`renderCoreProps` 用轻量 `CCSprite`
  （金币/钻石），避开 `Prop` 类 init 的 `CCBezierTo+actionCallBack` 自移动/拾取副作用。关键发现：①核心
  `Sp01State.animDir` 是 0-3 四象限（非英雄 1-8），新增 `coreSp01AnimDirToEight` 映射；②`CCSprite::setPosition`
  遮蔽基类 2 参重载（名称隐藏），道具壳用 `ccp()`；③`changeAction` 对 die/attack 一次性动作「锁住」等
  animationDone（已被屏蔽），核心切走时 `force` 打断。冒烟 PASS、corehash 4 帧两端逐位一致、`sp01=4`
  （怪已渲染）、无 `sog_crash.log`。
- ✅ **M3 完成（2026-09-15 完成，编译绿 + 冒烟验证）**：预测/回滚/快照纠偏接入 `StageScene`。`m_core` 换成
  `CPredictiveSim m_pred`（`BattlePrediction.h`）：`predictLocalFrame`（F1，本地立即消费本帧输入 + 远端
  最近已知输入 run-ahead 预测）→ `drainServerFrames`（F2，按 `LastConfirmedFrame()+1` 消费 `FRAME_NTF`
  权威帧 diff/回滚）→ `onSnapshot`（F3，`SNAPSHOT_NTF` → `ResyncFromSnapshot` 全量纠偏）。渲染源改读
  `m_pred.Core()`。冒烟 PASS：frame=100/150/200 两端 `corehash` **逐位一致**（同帧同哈希，确定性 + 回滚
  收敛验证）、`rollback=20~32`（远端输入预测未命中 → 回滚在跑）、`predicted=100/150/200`、`sp01=4`、
  `bullets=2`、无 `sog_crash.log`。期间修掉 `RollbackAndReplay` 陈旧 checkpoint bug（弹出条件 `> f`→`>= f`，
  `test_probe_stale.cpp` 回归 0 失配）。**HASH_NTF 上报暂缓**（服务器未注入碰撞/波次/配置，见 M1-c 依赖）。
- ✅ **M1-c 完成（2026-09-15 完成，冒烟验证）**：服务器注入碰撞+波次+配置（`si_battlecore_setup_default`
  一键注入 `GameData.h` 默认三件套，客户端 `exportCollisionToCore` 同步切 `MakeDefaultMapCollision()`），
  **HASH_NTF 上报已解除暂缓**：客户端 `UINetBattleLayer::sendHashNtf` 每 50 帧上报，服务器 `battle.lua`
  按帧归档哈希 + `handle_hash` 逐位比对。冒烟两端 frame=50/100/150/200 hash **逐位一致**、服务器
  `[Battle] hash ok`×4、无 mismatch。期间修掉 SWIG int64 返回值精度丢失（`lua_pushnumber` double 舍入 →
  `%typemap(out) int64/uint64` 用 `lua_pushinteger`，见 [[m1c-server-injection]]）。确定性闭环打通。
- ✅ **M5 技能接线（联网路径，2026-09-15 完成，冒烟验证）**：全屏炸弹技能打通核心。`skillButtonPressed`
  联网分支改为置「待触发标记」`m_nPendingSkill`（不再走旧 Cocos `doFullScreenBomb` 墙钟伤害路径）；
  `sampleNetInput()` 采集 `skill_id` 并清零（`m_nPendingSkill` 为 `mutable`，`sampleNetInput` 保持 const）；
  `renderCoreBombEffect()` 从核心 `HeroState::bombFuse` 推导表现——`0→>0` 播全屏闪光（`fullScreen03.png`
  缩放）、`>0→0` 播 8 向爆炸粒子+音效（`playOneFullBomb`），伤害由核心 `HurtSp01FullScreenBomb` 确定性结算。
  冒烟 PASS：frame=50/100/150/200 两端 `corehash` **逐位一致**（服务器 `hash ok`×4、无 mismatch）、无
  `sog_crash.log`；auto 测试在 frame 120 触发一次炸弹（`skill_id=1`）验证 skill_id 传播 + bombFuse 生命周期
  两端逐位一致。**期间修掉 `sizeof(StageScene)` 增量编译踩内存崩溃**（新增成员后旧 .obj 未跟头文件重编 →
  `setNetInputSink` 的 `std::function` 赋值 `_Tidy` 崩溃，`/t:Rebuild` 全量重编后消失）。
- 🔧 **两个实测 bug 修复（2026-09-15，核心侧单测绿）**：
  ①**行走摇杆松开后角色仍移动**——根因：`sampleNetInput()` 直接读 `m_pHud->getm_fMoveRadians()` 量化
  `move_dir`，但松开摇杆后 `m_fMoveRadians` 保留最后角度**不复位**（原单机靠 action 状态门控停止，联网输入
  没这层门控）。修复：`move_dir` 改用 `HudLayer::m_bIsHeld ? DirFromRadians(...) : 0` 显式门控（`StageScene.cpp`
  `sampleNetInput`），松开即 `0=停止`。
  ②**子弹有时打不中怪**——根因：核心 `UpdateBullet` 是「整帧一步 + 单点检测」（`BulletAddChangePos` 一次走完
  `speed` 再 `FxRect.Contains`），高速子弹（`kHeroBulletSpeed=20px/帧`，乃至 >32px/帧）会跳过 32×32 占位受击盒
  （隧道效应），且近身开火枪口出生点已在盒内时第一步就推出盒外。修复：`UpdateBullet` 三弹种（火箭筒/炮台弹/
  反怪弹）改「扫掠子步」——把单帧位移拆成 ≤8px 子步，**每个子步终点都做命中检测**（含出生点），`BulletAddChangePos`
  改子步版并返回剩余位移。`test_bullet.cpp` 新增测试 8（出生点在盒内命中）+ 测试 9（speed40>盒32 扫掠命中），
  全量回归（bullet/turret/bossai/skill/prop/config5/wave/hashverify/prediction/pacing）ALL PASS。
  **端到端冒烟 PASS（2026-09-15 完成）**：客户端编译绿 + `smoke_test.ps1 -AttachMode Off -KillAfter`
  退出 0（两玩家 battle frame≥200）；服务器 `[Battle] hash ok`×4（frame=50/100/150/200 无 mismatch）；
  客户端 `corehash frame=50/100/150/200` 两端 hash 逐位一致且与服务端 hash 对齐、`sp01=4`/`bullets=2`
  （怪与子弹已实际模拟）、`rollback=5→20`、无 `sog_crash.log`。两个 bug（摇杆松开仍移动 / 子弹打不中）
  端到端确认修复。
- 🔧 **两个新实测 bug 修复（2026-09-15，核心侧单测绿）**：
  ①**怪物攻击玩家不掉血**——根因：`Sp01AnimationDone` 的攻击分支（`kSp01Attack` / `kSp01BGuardAttack1/2` /
  `kSp01BGuardEquipAttack`）只切换动作，**从不调 `HeroDoHurt`**（只有老师接触伤害 `Sp01TeacherTouchHero`
  接线了英雄扣血），所以普通小怪/守卫贴脸攻击动画播完一轮也不结算伤害。修复：三个攻击分支在 `Sp01HurtHero`
  判定命中后接 `HeroDoHurt(*nearest, FxToIntRound(s.hurtToHero) * kHpScale, rng)`（守卫装备态 1.3 倍），
  `Sp01AnimationDone` 签名 `const std::vector<HeroState>&` → `std::vector<HeroState>&`（需可写）。
  ②**玩家武器与子弹初始位置不对齐（有偏移）**——根因：核心 `SpawnHeroBullet` 枪口偏移硬编码 `*20`，远小于
  实际枪口距离（从 `guns0.dat` 各枪 `fire_right` 帧 `rectRed` 枪口中心 + 身体 hand 挂点偏移 -3 校准：
  手枪 55 / 机枪 50 / 散弹 57 / 狙击 68 / 加特林 80 / 火箭筒 64），子弹出生在枪身里；且联网渲染路径跳过了
  `Hero::update`，`rightGunAndBodyPosition()` 每帧没补调，枪身节点与身体没对齐。修复：新增
  `GunToMuzzleOffset(gunType)` 按枪型给枪口偏移，`SpawnHeroBullet` 改读之；`renderCoreHeroes` 每帧补调
  `rightGunAndBodyPosition()`（复刻 `Hero::update` 里被跳过的调用）。垂直方向 ~12px 手高偏移因各朝向镜像
  不一致暂未建模（文档化，属后续 parity 细化项）。
  **核心单测绿（2026-09-15）**：`test_bullet.cpp` 新增测试 10（手枪/狙击开火子弹出生在枪口 55/68px）、
  `test_bossai.cpp` 新增测试 5（小怪贴脸 200 帧英雄掉血 + `hasGetHurtNum>0`）；全量回归
  （bullet/bossai/turret/skill/prop/config5/wave/hashverify/prediction/pacing/probe）ALL PASS。
  **端到端冒烟 PASS（2026-09-15 完成）**：客户端全量重编绿（`Hero::rightGunAndBodyPosition` 私有→公有，
  因访问修饰符改 mangled 名触发陈旧 obj 链接错，`//t:Rebuild` 解决）+ 服务端 `cmake --build build_msvc` 绿；
  `smoke_test.ps1 -AttachMode Off -KillAfter` 退出 0（两玩家 battle frame≥200）；两端 `corehash`
  frame=50/100/150/200 **逐位一致** 且与服务端 `[Battle] hash ok` 对齐（同帧同哈希，确定性未破）、
  `sp01=4`/`bullets=2`、`rollback=30→41`、无 `sog_crash.log`。
- ✅ **M4 炮台渲染壳完成（2026-09-15，单测绿 + 单机冒烟验证）**：炮台渲染壳 `renderCoreTurrets()` 落地——
  `Turret::createShell()/initShell()/renderShellSetAction()` 复用 `Turret` 类作纯表现（不走 `setTurretInfo`
  配置读崩溃 / 不挂 KeyFrameEvent 开火回调 / 不调度 `update` 自驱），动画由核心 `TurretState.action` 驱动
  （`kTurret*` 与 `Turret_ActionState_*` 值 1:1 直接强转）。数据源：`GameData.h` 新增 `MakeDefaultTurretSpawns`
  /`MakeDefaultTurretConfig`（占位 2 炮台，**仅注入单机路径 `initSingleCore`**，不动联网路径/服务端，联网
  `corehash` 不受影响）。`test_turret.cpp` 新增测试 7（默认炮台注入 → `SpawnInitialTurrets` 出生 → 确定性 +
  属性正确），全量回归（16 项）ALL PASS；客户端 `//t:Rebuild` 绿；`single_smoke_test.ps1` PASS（frame>=200、
  `turrets=2` 贯穿 frame=50/100/150/200、无 `sog_crash.log`）。
- ⚠️ **仍待办**：①M4-b 单机 parity（各关卡真实数据 `paota_point`/`enemyInfo.plist`/`out*.plist`/`level*.tmx`
  不在仓库，仍阻塞于 C6 配置 bin author）；②C6 S5 收口（客户端/服务器统一 `LoadBlobs` 注入核心、删
  `GameData.h` 五个 MakeDefault*、炮台真实激活波次）。

## 剩余路线图（2026-09-15 评估，含数据依赖关系）

- **炮台数据源已查清**：`Sp01Boss::initPTSp01s`（`Sp01Boss.cpp:731`）读 TMX `objectGroupNamed("paota_point")`
  对象层（每个对象 `name/x/y/plist` → `Turret::create(plist)`）。**联网地图 `level11.tmx` 无此对象层**（只有
  `collide_people/collide_bullet/entry/hero/drop_point` + `buildlayer*`），而单机地图 `level12/level15/level31…`
  **有** `paota_point`。→ 联网战斗**本来就没有炮台**（核心 `m_turrets` 空是正确行为），「炮台渲染」实为
  **M4 单机切核心**的子项：单机切核心时需把各关卡的 `paota_point` 数据注入核心（C6 配置 bin 一并 author）。
  **炮台渲染壳已用占位数据打通（2026-09-15）**：`renderCoreTurrets()` + `MakeDefaultTurretSpawns/Config`
  占位 2 炮台注入单机路径，单机冒烟 `turrets=2` 渲染无崩溃。真实 `paota_point` 数据仍待 C6 bin author 后替换。
- **M4 单机切核心与 C6 配置 bin 深度耦合**：单机每关的碰撞（不同 TMX）、波次（`out/out{level}.plist`）、
  小怪属性（`plist/enemyInfo.plist`）、炮台（`paota_point`）都是**关卡级数据**；而核心目前只有
  `MakeDefaultMapCollision/WavePlan/Sp01Config` 三个**共享默认占位**。M4 若现在做，只能让单机走共享默认
  → 所有关卡玩法雷同（parity 回归）。**结论：M4 的 parity 正确版依赖 C6 先把关卡数据 author 成 RSCFG bin**；
  在此之前 M4 只能「切核心 + 共享默认占位」的过渡态。此为下一个核心改动的**决策点**。
- **M5 技能接线（联网路径，✅ 已完成 2026-09-15）**：核心已完整支持全屏炸弹（`PlayerInput.skill_id=1` →
  `HeroState.bombFuse=10` 倒计时 → 到 0 结算 `HurtSp01FullScreenBomb`，半径 500 伤害 30HP；`=2` 冰冻占位未实现）。
  联网路径的缺口是**输入未采集 skill_id**（`sampleNetInput()` 恒置 0）+ **视觉仍走旧 Cocos `doFullScreenBomb`**
  （墙钟延迟 + 旧 RNG，非确定性）。M5 = 技能按钮 → 采集 `skill_id` → 核心结算 + 视觉改从核心 `bombFuse` 推导。
  **已知语义差异（文档化）**：旧 `doFullScreenBomb` 按技能升级等级出 1/2/3 波（`m_nTotalWaveNum`），核心 P2
  只做**单波**（一次 500 半径）；M5 联网先做单波，多波/升级等级属后续 parity 项。

## 0. 已核实的事实（Explore 结论，非提案）

- 客户端工程 `client/soulofgun/samples/HelloCpp/proj.win32/HelloCpp.vcxproj` **已链接**
  `shared/battlecore/BattleCore.cpp`，include 路径已含 `shared/`。**无需改构建系统**，核心已在 exe 里。
- `StageScene.h` 已 `#include "battlecore/BattleInput.h"`；核心的 `battlecore::CBattleCore` 已可用。
- 现状客户端真实战斗 tick 链（`StageScene::do_GameRunning`）：`m_pHero->update` / `m_pRemoteHero->update`
  / `m_pHud->update` / `updateAutoSkillAI` / `fireNetHeroes`(联网) / `m_pWeaponManager->update` /
  `m_pSp01Boss->update`(怪) / `m_pSp01PP->update`(粒子) / 相机 / zOrder / 寻敌箭头 —— **全是 Cocos 实体自驱**。
  联网确定性靠 `BattleDeterminism.h` 把 `random/frandom/CCRANDOM_0_1/millisecondNow` 重定向到
  `CBattleRng` + 逻辑帧时钟（`Defines.h`），**客户端↔客户端**一致，但**与服务器定点核心不同代码**。
- 服务器 `battle.lua` 已权威跑完整核心 + 锁步中继 `FRAME_NTF` + 哈希校准下发快照（`handle_hash`）。
- 已有可复用的「核心驱动渲染」样板：`UIBattleLayer.cpp`（`m_core.Init/SubmitInput/Tick` + `GetHeroes()`
  驱动 `MigSpriteNode` 身体/枪械 avatar + `onSnapshotNtf` 反序列化）。

## 1. 核心数据 → 渲染壳映射（已核对字段）

| 核心 `BattleEntity.h` | 渲染壳 | 映射 |
|---|---|---|
| `HeroState{x,y,action,animDir,alive,hp,maxHp,gunType}` | `Hero` | `setPosition(FxToFloat(x),FxToFloat(y))` + `changeAction(heroAction→ActionState_Hero, animDir→EightDirection)` + 血条 |
| `Sp01State{x,y,action,animDir,classId,hp}` | `Sp01` | 同上；`classId` 选 sprite/动画表 |
| `BulletState{x,y,rotate,classId,action}` | `Bullet` | `setPosition` + `setRotation(rotate)` + classId 选贴图 |
| `TurretState{x,y,aim,action}` | `Turret` | 同上 |
| `PropState{x,y,classId,action}` | `Prop` | 同上 |

动作枚举 `BattleEntity.h` 与 `ActionId.h` 值已一一对应（`kHeroStand=ActionState_Hero_stand` 等）。

## 2. 分里程碑（每个可编译 + 冒烟，绿了进下一个）

- **M1 联网切核心（锁步，无预测）**：`enterNetBattle` 初始化 `CBattleCore`（seed + 花名册 + 地图碰撞/出生点）；
  `doNetSimFrame` 改为「提交权威输入 → `m_core.Tick(frame)` → 从核心读英雄/子弹渲染」；暂不渲染怪/炮台/道具
  （核心已模拟，M2 补渲染）。`sync frame=` 打点改读核心。冒烟判据：两客户端 `hero=` 逐位一致、无崩溃、frame≥200。
- **M2 全实体渲染**：按 §1 映射把 `Sp01/Bullet/Turret/Prop` 从核心状态驱动（保留现有实体类作渲染壳，
  删其 `update`/AI/CCAction 伤害调用）。
- **M3 预测/回滚**：`CPredictiveSim` 接入 `StageScene`（本地立即消费输入渲染，`FRAME_NTF` diff→回滚）；
  接 `BATTLE_SNAPSHOT_NTF`（`ResyncFromSnapshot`）；`BATTLE_HASH_NTF` 上报**暂缓到 M1-c 之后**（服务器未注入
  碰撞/波次/配置，哈希必失配 → 快照风暴），M3 只做客户端侧 `HashAtConfirmedFrame` 冒烟打点（两端逐位一致）。
- **M4 单机切核心**：`GAME_STATE_RUNNING` 也走核心（采集摇杆输入 → `SubmitInput`/`Tick` → 渲染），
  删单机实体自驱路径（教学状态 GAME_STATE_TEACHING_* 暂留旧路径）。
- **M5 收尾**：skill 按钮 `skillButtonPressed` → `PlayerInput.skill_id=1`（全屏炸弹已在核心 `bombFuse`）；
  全屏炸弹/粒子/寻敌箭头等表现事件从核心状态推导；跑 `smoke_test.ps1` + P5 单机 parity 回归。

## 3. 风险

- **R-P3-1 视觉退化**：M1 阶段怪/炮台/道具不可见（核心已模拟但未渲染），联网画面暂时只有英雄+子弹。
  缓解：M1 只用于打通「客户端==服务器」确定性链路，M2 立即补全渲染；单机路径在 M4 前不受影响。
- **R-P3-2 教学状态**：`GAME_STATE_TEACHING_*` 大量依赖实体自驱 + CCAction 回调，M4 只切 `RUNNING`，
  教学状态暂留旧浮点路径（单机联网不混用，风险可控）。
- **R-P3-3 碰撞盒视觉对齐**：核心用「逻辑盒」判定，渲染壳仍按动画帧 rect；M2 需核对不产生肉眼可见错位
  （对齐 `multiplayer-design.md` R2）。
- **R-P3-4 相机/粒子/音效**：这些是纯表现，保留在 `StageScene`，但触发点从「实体回调」改为「核心状态/事件」。
