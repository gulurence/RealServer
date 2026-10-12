# M4 单机切核心 —— 实施计划

> 版本：v1（2026-09-15）
> 目标：单机 `GAME_STATE_RUNNING` 也走共享定点核心（采集摇杆输入 → `SubmitInput`/`Tick` → 渲染），
> 删掉单机实体自驱路径（`Hero::update`/`Sp01Boss::update`/`WeaponManager::update` 自模拟），
> 使单机与联网**同一份战斗逻辑**。教学状态 `GAME_STATE_TEACHING_*` 暂留旧路径。

## 状态（2026-09-15 实测更新）

- ✅ **M4-a 已实现（编译绿 + 联网冒烟回归 PASS）**：`initSingleCore()`（单英雄花名册 + 固定种子 20260915u +
  共享默认三件套注入）+ `update()` RUNNING 并入核心循环（单机直接 `SubmitInput`/`Tick`，不上报不预测回滚）+
  `do_GameRunning` 渲染早退条件放宽 `(m_bNetBattle || m_bSingleCore)` + 单机胜负判定（`allWavesCleared` 判胜 /
  英雄阵亡判负）+ `skillButtonPressed` 单机也走 `m_nPendingSkill` 核心结算。
- ✅ **联网冒烟回归 PASS**：`smoke_test.ps1` 双端 battle frame≥200（M4-a 改动不破坏联网路径——`update`/
  `do_GameRunning`/`skillButtonPressed` 的联网分支逐位等价）。
- ✅ **单机端到端冒烟 PASS（2026-09-15）**：补了 `--single` 自动路径（`AutoTest.h` 加 `single` 标志 +
  `UILoadingGameLayer` step5 分支进 `replaceToLoading`，选 big=0 small=2 → level13 避开 level11/12 教学门槛）。
  `EnvRoot/single_smoke_test.ps1` 起单客户端监控崩溃/进度，判据 = `[SingleCore] battlecore ready` 且
  （`frame>=200` 或 WIN/FAILED 终态）。实测 **PASS**：`battlecore ready map=1408x832 waves=3 cfg=16`、
  `frame=50/100/150/200` 逐 50 帧打点 `heroes=1 sp01=4 bullets>0`（单英雄 + 默认 3 波刷怪 + 开火产弹）、
  无 `sog_crash.log`。**期间修两个问题**：①`do_GameWin/do_GameFailed` 早退仍只判 `m_bNetBattle`，漏了
  `m_bSingleCore` → 单机胜负会走存档/解锁/对话框副作用（已改为 `m_bNetBattle || m_bSingleCore`）；
  ②`--single` 解析不进 exe → 增量编译陈旧 `.obj` 的 COMDAT 问题（`initAutoTest` 是头文件 inline 函数，
  旧 obj 无 `--single` 分支被链接），`/t:Rebuild` 全量重编后消失（见 [[client-build-cmd]]）。

## 0. 决策点（数据源）

单机每关数据（碰撞=不同 TMX、波次=`out/out{b}{s}.plist`、小怪属性=`Sp01.cpp` switch(classId) 硬编码、
炮台=`paota_point`）是**关卡级数据**，核心目前只有 `GameData.h` 三个共享默认占位。故 M4 分两阶段：

- **M4-a（本阶段，过渡态）**：单机切核心，用共享默认数据（`MakeDefault*`），**验证 wiring**（输入→核心→
  渲染→胜负闭环），parity 暂缓。单机画面「默认遭遇战跑在所选关卡地图上」，属已知退化。
- **M4-b（C6 之后，parity 版）**：关卡数据 author 进配置 bin 后，把碰撞/波次/小怪/炮台换成每关真实数据，
  跑 P5 单机 parity 回归。

本文件详述 **M4-a**；M4-b 依赖 C6，另行计划。

## 1. 已核实事实（file:line）

- 单机地图加载：`LoadingLayer.cpp:91` `IOBuiledLay::create("map/level{b+1}{s+1}.tmx")`；
  英雄创建于 `LoadingLayer.cpp:97-103`，出生点 `getm_pHeroPos()`（TMX hero 对象层）。
- 单机波次：`EnemyWaveManager.cpp:42` 读 `out/out{b+1}{s+1}.plist`，`darklayerba` 解析成时间线。
- 单机小怪属性：`Sp01.cpp:69` `switch(classId)` 硬编码 + `GameLevelData.h:48-50` 难度加成数组。
- 核心注入 API：`BattleCore.h` `SetWavePlan/SetSp01Config/MutableCollision`；`Init(seed, playerIds)`
  按索引给英雄定 id/出生点（`hero[i].id=i+1, x=160*(i+1)`）。
- 联网固定步长样板：`StageScene.cpp:774-803` `update()` 联网分支（`AdvanceBattleClock` →
  `sampleNetInput` → 上报/预测 → `drainServerFrames` → `do_GameRunning(kStep)`）。
- 渲染源已切核心：`renderCoreState/Heroes/Bullets/Sp01s/Props/BombEffect`（`StageScene.cpp:462+`），
  英雄按 `st.player_id == m_nNetPlayerId` 匹配 `m_pHero`/`m_pRemoteHero`。
- 单机自驱链：`StageScene.cpp:1411-1478` `do_GameRunning` 单机分支（`m_pHero->update`→`hud`→
  `updateAutoSkillAI`→`fireNetHeroes`(仅联网)→`weaponManager->update`→`sp01Boss->update`→相机→粒子→
  zOrder→`updateNearestSp01Dir`）。
- 胜负：`do_GameWin/do_GameFailed`（`StageScene.cpp:1147/1213`）已有联网早退分支；核心有 `CheckWin()`。
- 输入采样：`sampleNetInput`（`StageScene.cpp:230`）读 `m_pHud->getm_fMoveRadians/ShootRadians` +
  `HudLayer::m_bIsShooting` + `m_nPendingSkill`，已含 `sog::AutoTestConfig` 自动输入（可复用做单机冒烟）。

## 2. 改动方案（M4-a）

### 2.1 `StageScene.h` 新增成员

```cpp
bool    m_bSingleCore = false;   // 单机 GAME_STATE_RUNNING 也跑核心 (M4)
```

### 2.2 新增 `initSingleCore()`（初始化单机核心）

```cpp
void StageScene::initSingleCore()
{
    m_bSingleCore  = true;
    m_nNetPlayerId = 1;                          // 单机唯一英雄 id (固定)
    uint32_t seed  = 20260915u;                  // 固定种子 (单机无需跨机一致, 但求可复现)
    std::vector<int64_t> ids; ids.push_back(m_nNetPlayerId);
    m_coreReady = m_pred.Init(seed, ids);
    if (!m_coreReady) { /* log */ return; }
    exportCollisionToCore();                     // MakeDefault 碰撞 (M4-a 过渡态)
    m_pred.MutableCore().SetWavePlan(battlecore::MakeDefaultWavePlan());
    m_pred.MutableCore().SetSp01Config(battlecore::MakeDefaultSp01Config());
    battlecore::ResetBattleClock(0);
    m_nNetFrame = 0; m_fNetAccum = 0.0f;
}
```

调用时机：`update()` 首次进入 `GAME_STATE_RUNNING` 且 `!m_bNetBattle && !m_coreReady` 时惰性调用
（此时 `m_pHero`/`m_pHud` 已由 `LoadingLayer` 建好，`sampleNetInput` 可读摇杆）。

### 2.3 `update()` RUNNING 分支改造（单机并入核心循环）

现联网分支（`StageScene.cpp:774-803`）扩为「联网或单机核心」共用循环，单机差异仅两点：
不上报（无 `m_netInputSink`）、不预测/回滚（直接 `SubmitInput`+`Tick`）：

```cpp
if (m_nGameState == GAME_STATE_RUNNING && (m_bNetBattle || m_bSingleCore))
{
    m_fNetAccum += dt;
    const float kStep = (float)battlecore::kFixedDeltaMs / 1000.0f;
    while (m_fNetAccum >= kStep && m_nGameState == GAME_STATE_RUNNING)
    {
        m_fNetAccum -= kStep;
        battlecore::AdvanceBattleClock();
        m_nNetFrame = battlecore::CurrentFrameNo();
        battlecore::PlayerInput in = sampleNetInput();
        if (m_bNetBattle && m_netInputSink) m_netInputSink(in, m_nNetFrame);
        if (m_bNetBattle) predictLocalFrame(in);
        else { m_pred.Core().SubmitInput(in); m_pred.Core().Tick(m_nNetFrame); }
    }
    if (m_bNetBattle) drainServerFrames();
    do_GameRunning(kStep);      // renderCore 分支 (见 2.4)
    return;
}
```

### 2.4 `do_GameRunning` 渲染早退分支条件放宽

现 `if (battlecore::IsNetBattle() && m_coreReady)`（`StageScene.cpp:1414`）改为：
`if (m_coreReady && (m_bNetBattle || m_bSingleCore))` —— 单机也走 `renderCoreState()` + hud + 相机 +
粒子 + zOrder，跳过自驱链。单机自驱链（`1452-1476`）对 RUNNING 变死代码（教学状态仍走旧路径）。

### 2.5 胜负判定（读核心 `CheckWin()`）

在单机核心循环（2.3）`do_GameRunning(kStep)` 之后，读核心胜负：
`m_pred.Core().CheckWin()`（或英雄 `alive==false`）→ 触发 `do_GameWin()/do_GameFailed()`。
`do_GameWin/do_GameFailed` 现联网早退分支（`m_bNetBattle`）改为「`m_bNetBattle || m_bSingleCore`」
跳过单机存档/解锁/胜负对话框副作用（M4-a 单机胜负只切状态 + 弹框，存档/解锁属 parity 后续）。

### 2.6 技能按钮接核心（对齐 M5 联网路径）

`skillButtonPressed`（`StageScene.cpp:1000`）加单机核心分支：`m_bSingleCore` 时置
`m_nPendingSkill = battlecore::kSkillFullScreenBomb/kSkillFullScreenFrozen` 并 return（不再走旧
`doFullScreenBomb` 墙钟伤害路径）；视觉由 `renderCoreBombEffect` 从核心 `bombFuse` 推导（已就绪）。

### 2.7 英雄出生点对齐（M4-a 过渡态）

核心 `Init` 英雄出生 `x=160`；`renderCoreHeroes` 每帧把 `m_pHero` 覆盖到核心坐标。M4-a 接受
「单机英雄出生在核心默认点」（`LoadingLayer:99` 的 TMX 出生点被核心覆盖），M4-b 再对齐 TMX 出生点。

## 3. 验证（冒烟 + 回归）

1. **编译**：MSBuild（`//p:SolutionDir="D:\\code\\RealServer\\client\\soulofgun\\"`，禁止 taskkill）。
2. **单机冒烟**：复用 `sog::AutoTestConfig` 自动输入驱动单机（或手摇），判据：单机进入 RUNNING 不崩溃、
   英雄随输入移动/开火（读核心渲染）、`bullets>0` 帧、小怪按默认波次刷出、胜负可触发、无 `sog_crash.log`。
3. **联网回归**：`smoke_test.ps1` 双端 `corehash` frame=50/100/150/200 逐位一致、`hash ok`×4、无回退。
4. **教学状态回归**：`GAME_STATE_TEACHING_*` 仍走旧路径（本计划不动），确认不崩溃。

## 4. 风险与待办

- **R-M4-1 视觉退化**：M4-a 单机用默认碰撞/波次/小怪，与所选关卡地图不匹配（parity 退化）。缓解：这是
  过渡态，M4-b（C6 后）恢复每关数据；联网路径不受影响。
- **R-M4-2 单机胜负副作用**：`do_GameWin/do_GameFailed` 单机分支含存档/关卡解锁/难度递进/Android 回调，
  M4-a 先跳过（对齐联网早退），完整 parity 属 M4-b/P5。
- **R-M4-3 技能旧路径**：单机 `doFullScreenBomb` 多波/升级等级语义与核心单波不同（文档已记），M4-a 走
  核心单波，多波属 parity。
- **R-M4-4 教学状态依赖自驱实体**：本计划只切 RUNNING，教学状态保留旧浮点路径（单机/联网不混用）。
- **待办**：炮台数据源+渲染（M4-b 子项，单机关卡 `paota_point` → 核心 `m_turrets`）；C6 配置 bin
  接管数据桥；P5 单机 parity 回归。
