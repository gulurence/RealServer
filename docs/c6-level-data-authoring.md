# C6 关卡数据授权 —— 碰撞/波次/小怪/炮台 → 配置 bin + 内核注入（M4-b 前置）

> 版本：v1（2026-09-15）
> 目标：把「每关」的碰撞、波次编排、小怪属性、炮台位置从散落的源（TMX / out plist / enemyInfo plist /
> `Sp01.cpp` switch 硬编码）**离线导出为 RSCFG 配置 bin**，客户端/服务器各加载同一份注入定点核心，
> **删除 `shared/battlecore/GameData.h` 三个共享默认占位**。这是 M4-b（单机 parity）与 P5（单机 parity
> 回归）的前置——当前 M4-a 单机跑在所选关卡地图上但核心用 `MakeDefault*` 共享占位（已知退化 R-M4-1）。

## 0. 与既有 C0–C6 的关系

`config-system-design.md` 的 C0–C6 里，C0–C5 已完成（格式/导出器/读取器/CConfigMgr 热更/C4 客户端拉取/
C5 内核 `Sp01Config` 注入）。**本文档的「C6 数据授权」是 C5 的扩展**：C5 只做了 `Sp01Config`（小怪三属性），
尚未覆盖「碰撞」「波次编排」「炮台」。真正的「C6 热更闭环+战斗隔离」（运行时重载触发器 → Redis 发布 →
客户端拉取 → 下一局生效）是**正交的后续**——只有先把数据 author 进 bin，热更才有东西可更。故按依赖序：
**先本授权（打通 M4-b/P5）→ 后热更闭环**。

## 1. 数据源与现状（file:line 已核实）

| 数据 | 现源 | 现核心取值 | 目标 |
|---|---|---|---|
| 碰撞 `MapCollision{mapW,H,people[],bullet[],entries[]}` | TMX `collide_people`/`collide_bullet`/`entry` 对象层（每关不同） | `MakeDefaultMapCollision()` = level11 硬拷 | 每关 TMX → bin → 注入 |
| 波次 `WavePlan`（`WaveSpawnGroup`/`WaveDef`） | `out/out{b+1}{s+1}.plist`（Cocos plist，`EnemyWaveManager`/`darklayerba` 解析） | `MakeDefaultWavePlan()` = 3 波占位 | 每关 plist → bin → 注入 |
| 小怪属性 `Sp01Config{maxHp,walkSpeed,hurtToHero}` | `plist/enemyInfo.plist` + `Sp01.cpp:69` switch(classId) + `GameLevelData.h` 难度数组 | `MakeDefaultSp01Config()` = 16 classId 占位 | 每 classId → bin → 注入（C5 已支持注入） |
| 炮台 `TurretState` 位置/plist | TMX `paota_point` 对象层（仅单机关卡 `level12/15/31…`，联网 `level11` 无） | 空（无刷怪路径） | 每关 TMX → bin → 注入 + 刷怪 |

**关键事实**：服务器无 TMX/plist 解析器，故碰撞/波次/炮台必须**离线导出**成 bin（客户端/服务器同一份字节）；
不能在客户端运行时 TMX 直拷（那样服务器拿不到）。C5 的 `SetSp01Config` 注入通道已就绪，本文档扩展出
`SetWavePlan`（已有）/`SetMapCollision`（已有 `MutableCollision`）/`SetTurret`（新增）的 bin 数据源。

## 2. 工作分解（每步可编译 + 单测 + 冒烟）

### S1 波次 plist → bin 导出 + 内核读 bin（先行，C5 通道最顺）
- 新增离线导出：解析 `out/out{b}{s}.plist` 时间线 → `WaveSpawnGroup`/`WaveDef` 序列 → TSV（按
  `shared/config` 的 TSV 3 行头约定）→ `config_exporter` 打包 bin。
- 内核侧：`SetWavePlan` 已有；补「从 bin 行解码 `WavePlan`」的强类型表 `CWaveTable`。
- 单测：某关 plist → bin → 内核 `GetWavePlan()` 与手写期望逐字段一致 + 回滚安全（plan 不入快照）。

#### S1 详细设计（已定案 2026-09-15）

**数据源已逆向核实**（`Resources/res` zip 内 `out/out{bb}{ss}.plist`，共 48 关 `11..68`，纯文本 XML plist，
method=store 无需解密）：

- 顶层 `waveNum`(int)=波数、`outNum`(int)=每个 timeline 里最大 `outN` 序号（即刷怪通道/出生点序号上限）。
- `wave{w}.timelines[]`：每个 timeline 一个 `startTime`(string, 秒) + 若干 `out{i}` dict。
- `out{i}` = `{ delay(string, 60fps 帧计数), duilie(string 数组，有序怪物串) }`。
- 语义（`darklayerba`/`EBasa::createSp01`/`Sp01Boss::createASp01` 已核实）：
  - timeline 在波开始后 `startTime` 秒触发；触发时对每个 `out{i}` 的 `duilie[j]` 生成怪，
    第 j 个怪 `delay*j` 帧（60fps）后 appear（`m_nSp01PauseCount` 帧倒数）。
  - `out` 序号 `i` → 出生点 `entryPoint = i`（`getm_pEntrys()->objectAtIndex(i-1)`）。
  - 怪物串 `name` → 前缀决定 classId（子串匹配，顺序敏感，见 `Sp01Boss::createASp01`：boy_max→girl→dog→
    chef→dama→boy→police_max→police→worker_max→zombie_max→zombie→worker→boss01→boss03→boss2→zhizhu）；
    `_` 后缀 = `Sp01Info` 行号 `sp01Id`（`getSecondSplit_OfChar`，如 `boy_511`→511、`dog_0`→0）。
  - 特殊串：`pt_{n}`=炮台（S4，本步跳过）、`drop_{n}`=掉落点、`dialog_{...}`=对话，均非刷怪，本步跳过并记录。
  - `isKey` 字段在 48 关 plist 中**全部缺失** → 恒 false（只影响显示波次 `xianShiWaveNum`，内核胜负用
    `allWavesCleared` 不依赖它）。

**转换口径（记录，P5 parity 时校准）**：`startTime`(秒)→`startTime`(帧)=`round(×20)`；`delay`(60fps 帧)→
`interval`(20Hz 帧)=`(delay+1)/3`（round-half-up）。**异质 duilie**：按「连续相同 `(classId,sp01Id)` run」
拆成多个 `WaveSpawnGroup`，run 首元素相对时间 = `round(runStartIndex*delay/3)` 叠加到 timeline `startTime`。

**bin 组织（对齐 R-A5 一张大表按 levelId 索引）**：两张表 `wave` + `wave_spawn`，全部 48 关合成一个 bin；
`levelId = big*10+small`（11..68）。行 `id` 用导出顺序的自增序号（`ForEachId` 升序即 level→wave→seq 序）。

- `wave`：`id/levelId/waveNum/isKey`。
- `wave_spawn`：`id/levelId/waveNum/seq/classId(u8)/sp01Id/count/interval/entryPoint/startTime`。

**新文件**：`shared/config/WavePlist.h`（纯 C++ plist XML 解析 + plist→`std::vector<WaveDef>` 映射 +
`WaveDef`→`TableData`）、`shared/config/CWaveTable.h`（`WaveInfo`/`WaveSpawnInfo` 行结构 + 跨两表
`GetWavePlan(levelId)` 解码）、`tools/wave_exporter/wave_exporter.cpp`（CLI：plist→bin+生成头）、
`shared/config/test/test_wavebin.cpp`（单测：out13 plist→bin→`GetWavePlan(13)` 与手写期望逐字段一致 +
`plan 不入快照` 回滚安全）。

#### S1 完成（2026-09-15，冒烟已过）

- **代码落地**：上述 3 个实现文件 + `build_wave_exporter.bat`/`build_wavebin.bat`（MSVC `/std:c++14
  /utf-8`，经 PowerShell 起 `vcvars64` 编译）。
- **单测**（`test_wavebin`，8 项全 PASS）：合成 plist（多波/异质 `boy_10,boy_511`、`pt_1,girl_11`、
  `drop_1,boss01_0,dialog_7,8`/不同 delay 40·60·80、startTime 0.1·5·1 秒）→ `PlistToWavePlan` == 手写期望
  逐字段；同 plist → `WavePlanToTables`→`BuildBin`→`CBinReader.Load`→`CWaveTable.GetWavePlan(11)` == 手写期望；
  不存在的关卡返回 false。
- **冒烟**（真实 48 关）：`wave_exporter out/out*.plist` → 69146 字节 bin（`wave` 199 行 + `wave_spawn` 1303 行），
  逐一 `GetWavePlan(levelId)` 解码回 wave/组数与导出器直解析逐关一致（`关11:1波3组 … 关68:5波45组`）；另 out13/11/68/17
  `direct 解析 == bin 往返` 逐字段自洽。**通过**。
- **回滚安全**：`BattleCore.h:120` 标注 `m_wavePlan`「配置注入, 本局常量, 不入快照」；`SerializeState`/`HashState`
  只覆盖 `m_wave`（`waveNum/xianShiWaveNum/gamePauseTime/waveTimer/allWavesCleared`），不含 `m_wavePlan`。
- **修复**：`WavePlanToTables` 增加 `waveIdBase/spawnIdBase` 参数（多关卡合成大表避免行 id 冲突），exporter 持
  全局自增计数；`wave_exporter.cpp` 补 `#include CBinWriter.h`（原缺，`BuildBin` 未声明）。

**待 S5 收口**：bin 产物落点（客户端 `LoadBlobs` 与服务器注入）未接；S1 只打通「plist→bin→内核读」范式，
未接入运行时注入通道（`SetWavePlan` 已有，S5 统一 `LoadBlobs→注入` 时接）。

### S2 碰撞 TMX → bin 导出 + 内核读 bin
- 离线导出：解析 TMX `collide_people/collide_bullet/entry` 对象层 → `MapCollision`（矩形/出生点）→ bin。
- 客户端 `exportCollisionToCore` 改「读 bin」而非 `MakeDefaultMapCollision`；服务器 `battle.lua::ensure_core`
  同样注入（`si_battlecore_setup_*` 改传 bin 字节或先 LoadBlobs）。
- 单测：level11 TMX → bin → 与现 `MakeDefaultMapCollision`（已 dump→author 保证一致）逐字段一致。

#### S2 详细设计（已定案 2026-09-15）

**数据源已核实**（`Resources/res` zip 内 `map/level{bb}{ss}.tmx`，48 关 `11..68`，纯文本 XML TMX）：
- 48 关**全部**含 `collide_people`/`collide_bullet`/`entry` 三个对象层（grep 逐一核实 people/bullet/entry 恒=1）；
  `paota_point`（S4）只出现在约 18 个单机关卡，本步不碰。
- `<map width=22 height=13 tilewidth=64 tileheight=64>` → `mapWidth=width*tilewidth`、`mapHeight=height*tileheight`
  （level11 = 1408×832，与 `MakeDefaultMapCollision` 一致）。

**坐标换算（`CCTMXXMLParser.cpp:513/523/526` + `IOBuiledLay.cpp:addCollides/addEntrys` 已核实）**：
- 对象层无 `offsetx/offsety` → 偏移 (0,0)。
- 矩形（people/bullet）：`x = rawX`（可负，如 -716）；`y = mapHeight - rawY - rawHeight`；`w/h` 不变。
- 出生点 entry：客户端 `addEntrys` 读 `y = (mapHeight - rawY - rawH) + rawH = mapHeight - rawY`，`x = rawX`。
  （即 entry 取对象**顶边** y，矩形取**底边** y。）用 level11 三个 entry 与 2 矩形逐项回代 `MakeDefaultMapCollision`
  已完全吻合（如 collide4 raw(820,444,216,130) → (820, 832-444-130=258, 216,130)；entry1 raw(1382,362) →
  (1382, 832-362=470)）。

**顺序（影响快照，两端必须一致）**：
- people/bullet 矩形 = **文件序**（客户端 `getObjects()` 解析序）。
- entry 出生点 = **按名字数字序** `entry1,entry2,…`（客户端 `objectNamed("entry%d")` 循环）。

**bin 组织（对齐 S1 大表按 levelId 索引）**：三张表 `map` + `map_rect` + `map_entry`，48 关合成一个 bin。
- `map`：`id/levelId/mapWidth/mapHeight`。
- `map_rect`：`id/levelId/kind(u8:0=people,1=bullet)/seq/x/y/w/h`（x/y/w/h = 已换算的 cocos 像素 int）。
- `map_entry`：`id/levelId/seq/x/y`。

**新文件**：`shared/config/TmxCollision.h`（纯 C++ TMX 对象层解析 + TMX→`MapCollision` 映射 +
`MapCollision`→`TableData`）、`shared/config/CMapTable.h`（`MapInfo`/`MapRectInfo`/`MapEntryInfo` 行结构 +
跨三表 `GetMapCollision(levelId)` 解码，`FxFromInt` 转定点）、`tools/tmx_exporter/tmx_exporter.cpp`
（CLI：`level{bb}{ss}.tmx`→bin，levelId 从文件名末尾数字推导）、`shared/config/test/test_mapbin.cpp`
（单测：level11 TMX → bin → `GetMapCollision(11)` 与 `MakeDefaultMapCollision()` 逐字段一致）。

#### S2 完成（2026-09-15，冒烟已过）

- **代码落地**：`TmxCollision.h`/`CMapTable.h`/`tmx_exporter.cpp` + `build_tmx_exporter.bat`/`test_mapbin.cpp`/
  `build_mapbin.bat`。
- **单测**（`test_mapbin`，7 项全 PASS）：合成 TMX（2 people + 1 bullet + 乱序 entry2/entry1）→
  `ParseTmxCollision` == 手写期望逐字段（矩形 y 底左换算、entry 顶边换算、entry 数字序排序均验证）；合成 bin
  往返 == 手写期望；空 reader 返回 false；**真实 level11.tmx `direct == MakeDefaultMapCollision()` 逐字段 +
  bin 往返 == 同一**（people=29/bullet=35/entry=3）。
- **冒烟**（真实 48 关）：`tmx_exporter map/map/level*.tmx` → 85603 字节 bin（`map` 48 行 + `map_rect` 1877 行 +
  `map_entry` 217 行），逐一 `GetMapCollision(levelId)` 解码 48 关全非空（全部 1408×832，entry 数 2~8 随关卡变）。
  **通过**。
- **确认**：坐标换算与 `CCTMXXMLParser.cpp:523-526` 完全一致（level11 三 entry + 若干矩形回代 `MakeDefaultMapCollision`
  逐位吻合），矩形文件序 / entry 名字数字序对齐客户端 `getObjects()`/`objectNamed("entryN")`。

**待 S5 收口**：客户端 `StageScene::exportCollisionToCore`（现 `col = MakeDefaultMapCollision()`，`StageScene.cpp:224-227`）
改「读 bin → `CMapTable::GetMapCollision(levelId)`」；服务器 `battle.lua::ensure_core` 同样注入。S2 只打通「TMX→bin→内核读」
范式，未接入运行时注入。

### S3 小怪属性 enemyInfo → bin（替换 `Sp01.cpp` 硬编码 + 难度数组）
- 离线导出：`plist/enemyInfo.plist` + `GameLevelData.h` 难度加成 → 每 classId `Sp01Config` → bin。
- C5 已支持 `SetSp01Config`，只需把 bin 行接入替换 `MakeDefaultSp01Config`。
- 单测：16+ classId 属性与手写期望一致。

#### S3 详细设计（已定案 2026-09-15）

**数据源**：
- `plist/enemyInfo.plist`：126 个键 `{name}_{N}` → dict，字段 `hp`(int，以 `<string>` 存)、`moveSpeed`(float)、
  `hurtHPToHero`(float)、`coins`/`sunshine`(dict)、`huojiantong`/`jiatelin`(int)、`nameInMig`(string)。
  **N 是每 name 内序号**：`boy_511` 与 `girl_511` 并存，N 不全局唯一。
- `GameLevelData.h:48-50` 难度加成数组：`Sp01_HP_INCREASE[3]={0,50,100}`、`Sp01_HURTHERO[3]={0,5,15}`、
  `Sp01_MOVE_SPEED[3]={0,0.1,0.1}`。

**关键结论（已实测）**：
1. **N 不全局唯一 → 内核 `Sp01Config` 键必须从「sp01Id 单键」升级为「(classId,sp01Id) 组合键」**。
   实测 `boy_511` hp=250 vs `girl_511` hp=200（同为 sp01Id=511），单键会互相覆盖。
   组合键 `Sp01ConfigKey(classId, sp01Id) = (classId<<16)|sp01Id`（classId<16、sp01Id<65536，安全）。
2. **走速换算**：客户端 `m_fWalkSpeed=(moveSpeed+难度+抖动)*60`(px/s) → 内核 20Hz px/帧 = `×60/20 = ×3`。
   `walkSpeed = FxFromFloat((moveSpeed + kSp01DiffSpeed[diff]) * 3.0f)`（hero 同口径：137.5px/s÷20=6.875）。
3. **难度系数 = 运行时本局常量**（R-A2 定为「注入时乘难度系数」，不预生成 3 套 bin）：由配置层
   `CSp01Table` 按 difficulty(0/1/2) 应用常数增量后一次性 `SetSp01Config`，内核零运行时表。
4. **非确定性抖动丢弃**：客户端 `Sp01::rebirth` 的 `random(100)*0.001` 走速抖动破坏确定性，S3 不导出；
   内核走速只取 `(moveSpeed+difficulty)×3`（行为随机性由内核 `CBattleRng` 别处承担）。

**bin 组织**：小怪属性**全局一份**（非按关卡），`sp01` 单表：
```
sp01 (id/classId/sp01Id/hp/moveSpeed/hurtHPToHero)
  字段类型: U32/U8/I32/I32/F32/F32   ← 存原始 plist 值, 换算在解码层 (对齐 CMapTable 存像素 int 范式)
```
- `classId`：复用 `WavePlist.h::ParseMonsterId` 的 name→classId 映射（16 类全覆盖；`ptinfo_*` 判
  special 跳过，属 S4 炮台）。
- `sp01Id`：`_N` 后缀 strtol。
- 键 = `Sp01ConfigKey(classId, sp01Id)`；`BuildConfigMap(difficulty)` 产出 `std::map<uint32_t,Sp01Config>`
  供 `SetSp01Config` 注入。

**换算口径**（解码层 `CSp01Table::GetSp01Config`，难度增量常数 `kSp01DiffHp/Hurt/Speed[3]` 与
`GameLevelData.h` 逐位对齐）：
```
maxHp      = hp + kSp01DiffHp[diff]                                   // 整数 HP (内核 ×kHpScale=100 进 centi)
walkSpeed  = FxFromFloat((moveSpeed + kSp01DiffSpeed[diff]) * 3.0f)   // px/帧 @20Hz
hurtToHero = FxFromFloat(hurtHPToHero + kSp01DiffHurt[diff])          // Q16.16 HP (可为小数 0.5)
```

**内核改动（组合键，最小侵入）**：
- `BattleEntity.h`：新增 `inline uint32_t Sp01ConfigKey(uint8_t classId, int32_t sp01Id)`。
- `BattleCore.cpp::SpawnSp01`：`cfg->find((uint32_t)idInSp01Info)` → `cfg->find(Sp01ConfigKey(classId, idInSp01Info))`。
- `GameData.h::MakeDefaultSp01Config`：键 `(uint32_t)classId` → `Sp01ConfigKey(classId, classId)`
  （与 `MakeDefaultWavePlan` 的 `sp01Id=classId` 占位一致，过渡期不破坏现有单机/联网路径）。
- `test_config5.cpp`：`MakeConfig()` 改组合键 + 新增「同 sp01Id 不同 classId 碰撞解析」用例。

**交付物**：
- `shared/config/CSp01Table.h`（解码 + 难度增量常数 + `BuildConfigMap`）
- `shared/config/EnemyInfoPlist.h`（plist → sp01 表 的离线映射，`EnemyInfoToSp01Table`）
- `tools/sp01_exporter/sp01_exporter.cpp` + `build_sp01_exporter.bat`
- `shared/config/test/test_sp01bin.cpp` + `build_sp01bin.bat`（合成 plist 手写期望 + 真实 enemyInfo 冒烟 + 组合键碰撞）

#### S3 完成（2026-09-15，冒烟已过）

- **内核组合键落地**：`BattleEntity.h` 新增 `Sp01ConfigKey(classId,sp01Id)=(classId<<16)|sp01Id`；
  `SpawnSp01` 改按组合键查 `Sp01Config`；`MakeDefaultSp01Config` 键改 `Sp01ConfigKey(classId,classId)`
  （与占位波次组 `sp01Id=classId` 一致，过渡期不破坏客户端/服务器默认路径）。
- **单测**（`test_config5`，7 项全 PASS）：原 5 项 + 新增「同 sp01Id 不同 classId 碰撞解析」
  （`boy_511` hp=25000 vs `girl_511` hp=20000 各自取各自配置，不再互相覆盖）。
- **解码层**（`CSp01Table` + `EnemyInfoPlist`）：单测 `test_sp01bin`（合成 5 项 + 真实 6 项全 PASS）：
  - 换算 `maxHp=hp`、`walkSpeed=FxFromFloat(moveSpeed×3)`、`hurtToHero=FxFromFloat(hurt)` 逐位一致；
  - 难度增量 0/1/2 的 hp+{0,50,100} / hurt+{0,5,15} / speed+{0,0.1,0.1} 逐位一致；
  - 组合键碰撞 `boy_511=250 vs girl_511=200` 不覆盖；`_max` 双下划线 `boy_max_53 → sp01Id=53`。
- **真实数据冒烟**：`sp01_exporter /tmp/sogout/plist/enemyInfo.plist` → 4137 字节 bin，**120 行 + 跳过 6**
  （跳过 = `ptinfo_0..5` 炮台，属 S4）；真实 `boy_11` hp=30/hurt=0.5、`boy_511=250`/`girl_511=200` 抽查一致。
- **回归修复（S1 `ParseMonsterId`）**：发现并修复 `id.find('_')`→`id.rfind('_')`——原实现把 `boy_max_53`
  的 sp01Id 解析成 0（取首个下划线后 `"max_53"`），导致所有 `_max` 怪配置互相覆盖。修复后重导出 wave.bin：
  wave_spawn **1303 → 1470 行**（`_max` 连续 run 因 sp01Id 变不同而正确拆分）；`test_wavebin` 合成
  plist 增补 `boy_max_53` 用例锁死 rfind；48 关 `GetWavePlan` 全解码 + `boy_max` 存在非零 sp01Id
  （样例 50）通过。S2 `test_mapbin`（level11 29/35/3）复跑无回归。

**待 S5 收口**：客户端 `StageScene.cpp:211/252`、服务器 `luacore_battle.i:160` 现仍用
`MakeDefaultSp01Config()`，S5 改「读 sp01.bin → `CSp01Table::BuildConfigMap(difficulty)` → `SetSp01Config`」；
难度在本局开始由框架选定（默认 0）。

### S4 炮台 paota_point → bin + 内核刷怪/渲染
- 离线导出：TMX `paota_point` 对象层（`name/x/y/plist`）→ `TurretState` 出生 → bin。
- 内核：`SetTurret` 注入 + `Tick` 刷炮台（现 `m_turrets` 无刷怪路径）。
- 渲染：`renderCoreSp01s` 已预留炮台分支（`kClassTurret` 走 `GetTurrets()`），补 `renderCoreTurrets`。
- 单测：某单机关卡 `paota_point` → bin → 内核 `GetTurrets()` 位置/plist 一致。

#### S4 详细设计（已定案 2026-09-15）

**数据源**：
- TMX `paota_point` 对象层（17 关：12,13,15,18,22,25,27,31,33,36,38,42,44,47,62,65,68）：
  `<object name="pt_N" x y width height>`（**非自闭合**，内嵌子属性）+
  `<property name="plist" value="ptinfo_N"/>`。每关 1~8 个炮台；`level62` 有重复 `pt_1`（数据瑕疵）。
- `enemyInfo.plist` 的 `ptinfo_0..5`（S3 已判 special 跳过，属炮台）：`hp`(500~5000)、`moveSpeed`(2~2.5)、
  `hurtHPToHero`(1)、`attackdis`(300)、`coins`/`sunshine`(dict)、`nameInMig`("boy")。
- 波次 plist `out*.plist` 的 `pt_N` 条目（S1 已跳过）：duilie 里 `pt_N` 激活同名炮台。

**关键结论（已实测）**：
1. **炮台坐标 = 客户端 CCTMXXMLParser 已翻转后的值**：`x = rawX`，`y = mapHeight - rawY - rawHeight`
   （`CCTMXXMLParser.cpp:526`，与碰撞矩形同公式；`initPTSp01s` 直接 `setPosition(x,y)` 不再二次翻转）。
2. **炮台配置无难度加成**：`setTurretInfo`(`UISetLayerb.cpp:1039`) 只读 raw 值，不套 `Sp01_HP_INCREASE`
   等（难度数组只作用于 `Sp01::rebirth`）。故 `turret` 表不乘难度。
3. **炮弹速度 ×3**：客户端 `createAPaoTaB(..., m_fWalkSpeed=moveSpeed)` 直接作子弹速度（60fps px/帧），
   内核 20Hz = `moveSpeed × 3`（与 S3 走速同口径，但**无难度增量、无抖动**）。
4. **炮台激活 = 波次驱动**：duilie 里 `pt_N` 在第 j 位 → 激活帧 = `timelineStart + round(j*delay/3)`，
   appear 延迟 = 同一 `round(j*delay/3)`（`activeThisTurret(delayDelta*j)` 的 m_fDelay，60fps→20Hz）。
   用 `wave_spawn` 行 `classId=kClassTurret(16)` 哨兵 + `sp01Id=N`(nameIndex) 表达；`count=1`
   （`WaveGroupSpawnedAt` 对 count=1 恒在 startTime 触发一次），`interval` 复用承载 appear 延迟。
5. **掉落配置延后**：内核 `TurretState` 有 drop 字段但 S4 不导出（与 S3 `Sp01Config` 最小集对齐，
   sp01 小怪当前同样不产出掉落）；炮台死亡掉金币/阳光属 P5 parity 统一补。

**bin 组织**（沿用「一张大表按 levelId 索引」）：
```
turret (id/ptinfoId/hp/moveSpeed/hurtHPToHero/attackdis)            ← enemyInfo.plist ptinfo_0..5, 全局一份
  字段类型: I32/I32/I32/F32/F32/I32   ← 存原始值, 换算在解码层
turret_point (id/levelId/nameIndex/seq/x/y/ptinfoId)                ← TMX paota_point, 按 levelId 索引
  字段类型: I32/I32/I32/I32/I32/I32/I32
```
- `nameIndex` = `pt_N` 的 N；`ptinfoId` = `ptinfo_N` 的 N；`x/y` 存翻转后像素 int（`y=mapHeight-rawY-rawHeight`）。
- 波次激活不新增表：`pt_N` 直接进现有 `wave_spawn`（`classId=16` 哨兵、`sp01Id=N`、`count=1`、
  `interval=appear延迟`、`startTime=激活帧`）。

**换算口径**（解码层 `CTurretTable::BuildTurretConfigMap`）：
```
maxHp      = hp                                    // 整数 HP（内核 ×kHpScale=100 进 centi）
hurtToHero = FxFromFloat(hurtHPToHero)             // Q16.16
bulletSpeed= FxFromFloat(moveSpeed * 3.0f)         // px/帧 @20Hz
hurtHeroDis= attackdis                              // 像素距离（FxDistPx 直接比较）
```

**内核改动**：
- `BattleEntity.h`：新增 `struct TurretConfig{maxHp,hurtToHero,bulletSpeed,hurtHeroDis}`、
  `struct TurretSpawn{nameIndex,x,y,ptinfoId}`（x/y 为 fx_t）。
- `BattleCore.h/.cpp`：`SetTurretSpawns(vector)` + `SetTurretConfig(map<int,Config>)` 注入
  （本局常量，不入快照）；`Tick` 首帧幂等 `SpawnInitialTurrets()`——按 `m_turretSpawns` 文件序
  生成 `kTurretInit` 状态炮台（`TurretState.id` 走 `m_nextId` 分配，属性按 `ptinfoId` 查
  `m_turretConfig`）；`UpdateWave` 对 `classId==kClassTurret` 的组改为
  `ActivateTurret(sp01Id=N, delay=interval)`（现 `SpawnSp01` 只处理小怪）。
- `WavePlist.h::PlistToWavePlan`：`pt_N` 由「special 跳过」改为生成 kClassTurret 哨兵组
  （`drop_`/`dialog_` 仍跳过）。

**交付物**：
- `shared/config/CTurretTable.h`（turret + turret_point 解码 + `BuildTurretConfigMap`/`GetTurretSpawns`）
- `shared/config/EnemyInfoPlist.h` 增 `PtInfoToTurretTable`
- `shared/config/TmxCollision.h` 增 `ParseTmxTurretPoints` + `TurretPointsToTables`
- `tools/sp01_exporter` 增输出 `turret` 表；`tools/tmx_exporter` 增输出 `turret_point` 表
- `shared/config/test/test_turretbin.cpp` + `build_turretbin.bat`

#### S4 完成（2026-09-15，配置层 + 内核端到端 冒烟已过）

- 内核侧（上一会话已落地）：`TurretConfig`/`TurretSpawn` 结构 + `SetTurretConfig/SetTurretSpawns` +
  `SpawnInitialTurrets`（首帧幂等，属性按 `ptinfoId` 查表，无配置回退默认）+ `ActivateTurret`
  （同名「后者胜」对齐 CCDictionary 覆盖语义）+ `UpdateWave` 的 `kClassTurret` 哨兵分支 +
  序列化/哈希增 `nameIndex`（回滚安全）。`WavePlist.h` `pt_N` 由「special 跳过」改为生成
  `kClassTurret` 哨兵组（`sp01Id=N`、`count=1`、`interval=appear延迟`、`startTime=激活帧`）。
- 配置层（本会话落地）：`PtInfoToTurretTable`（`ptinfo_N`→`turret` 表，schema
  `id/ptinfoId/hp/moveSpeed/hurtHPToHero/attackdis`）、`ParseTmxTurretPoints`（`paota_point` 非自闭合
  object + 内嵌 `<property name="plist">` → nameIndex/x/y(翻转)/ptinfoId）、`TurretPointsToTables`、
  `CTurretTable`（`BuildTurretConfigMap` 换算 `hurtToHero=FxFromFloat(hurtHPToHero)` /
  `bulletSpeed=FxFromFloat(moveSpeed×3)` / `hurtHeroDis=attackdis`；`GetTurretSpawns` 按 levelId 重组）。
- 导出器：`sp01_exporter` 增 `turret` 表（与 `sp01` 同 bin）；`tmx_exporter` 增 `turret_point` 表
  （第 4 表；`paota_point` 无的关卡跳过不报错）。
- 冒烟（合成数据，`test_turretbin` ALL PASS）：PtInfoToTurretTable 2 行/boy_11 跳过；turret bin 往返
  属性逐位一致；ParseTmxTurretPoints y 翻转正确；turret_point bin 往返 spawn 逐位一致；内核端到端
  「注入配置+波次哨兵 → 首帧出生 2 炮台 + pt_1 激活 appear / pt_2 保持 init」正确。
- 回归：`test_sp01bin`/`test_mapbin` ALL PASS；`test_wavebin` 手写期望补 `pt_1→kClassTurret` 组后
  ALL PASS（上一会话 WavePlist.h 的 `pt_N` 语义变更未同步改该单测，本会话补齐）。
- **数据更正（2026-09-15）**：真实数据其实在客户端 `Resources/res`（3.3MB zip）内，解出后位于 `/tmp/sogout`
  （`C:\Users\...\Temp`，不在 D: 盘，此前「不在仓库内」是误判）。已再授权三 bin 到 `shared/config/data/`：
  `wave.bin`(199+1470 行) / `sp01.bin`(sp01 120 + turret 6) / `map_collision.bin`(map 48 + map_rect 1877 +
  map_entry 217 + turret_point 87，覆盖 17 关 paota_point)，源数据副本在 `shared/config/data/source/`。
  客户端 `renderCoreTurrets`（S4 渲染壳）已完成（占位数据打通，待 S5 接真实 bin）。炮台死亡掉落配置延后至 P5 parity。

### S5 收口 + 删除 `GameData.h`
- 客户端/服务器统一「读关卡 bin → 注入核心」；删除 `shared/battlecore/GameData.h` 五个 `MakeDefault*`
  及 `si_battlecore_setup_default`，改走真实关卡 bin。
- 回归：单机 `single_smoke_test.ps1` 各关卡 parity 采样 + 联网 `smoke_test.ps1` 双端 hash 逐位一致 + P5。
- **进度（2026-09-15）**：✅ **S5 已落地并验证**。新增 `shared/config/LevelDataLoader.h`（`InjectFromDir` 一次
  注入碰撞+波次+小怪+炮台）；客户端 `StageScene::injectLevelDataToCore`（联网固定 levelId=11、单机按
  `UISetLayerb` 选关算 levelId/difficulty）、服务器 SWIG `si_battlecore_setup_level`、`battle.lua` 两处切换。
  **`GameData.h` 已删除**（`test_turret` 删测试 7、`test_mapbin` 测试 3 改维度/往返校验）。bin 部署到客户端
  `Debug.win32/config/`。验证全绿：`test_leveldata`(11/12/68/9999)、`test_mapbin`(level11.tmx 真实)、
  `test_turret`、单机冒烟(level13 `waves=4 cfg=120 turrets=2` frame≥200)、联网冒烟 corehash frame 200/250/300
  两端逐位一致。**注意**：本步用本地 `CBinReader::LoadFile` 直读 bin（非 `LoadBlobs` 内存注入），C4 热更
  CONFIG_PULL 通道接入留待后续（见 §4 决策点）。联网 levelId/difficulty 硬编码 levelId=11 已由 S6 解耦。

### S6 levelId 匹配流流入
- 目标：消除 `battle.lua` 与客户端 `StageScene` 的硬编码 `levelId=11`，让关卡/难度从房主创建房间起
  流经「Room→Redis→Battle→ENTER_RSP→客户端」，两端注入同一关卡 bin。
- 链路：`PBRoomCreateReq.level_id(7)/difficulty(8)` → `room.lua handle_create` 校验(11..68/0..2)存储 →
  `handle_start` 写 `battle:level:{battleId}="levelId,difficulty"` → `battle.lua ensure_core` 读回注入
  （`save_replay` 回放校验核复用同 level）→ `PBBattleEnterRsp.level_id(8)/difficulty(9)` 权威回传 →
  客户端 `UINetBattleLayer::onEnterRsp` 读 level → `StageScene::initBattleCore` 注入替代硬编码。
- **进度（2026-09-16）**：✅ **S6 已落地并验证**。改 10 文件：`room.proto`/`battle.proto`(make.bat 重生成)、
  `luacore_battle.i`(`si_pb_battle_enter_rsp` 加 level/diff)、`room.lua`(读存 + 写 Redis)、`battle.lua`
  (读 Redis + 回传 + 回放校验)、客户端 `AutoTest.h`(`--level`)、`UILobbyLayer.cpp`(建房上送)、
  `UINetBattleLayer.cpp`(读回)、`StageScene.h/.cpp`(传参注入)。冒烟 `smoke_test.ps1 -Level 13` PASS，
  服务端 `core init ... level=13 diff=0` == 客户端 `battlecore ready ... level=13 diff=0`，frame 100/150/200
  hash 逐位一致；`-Level` 缺省回退 level 11 也 PASS 且 0 mismatch。**观察**：level 13 在地图障碍附近会触发
  一次 frame-50 瞬时预测漂移（两客户端互相同意、与服务器不同），F3 `snapshot resync` 自动校准后 100/150/200
  复一致 —— 属预测/回滚框架既有特性（错误预测的远端输入在 level 13 障碍下影响更显著），非 level 流动回归。
- 缺口（留待后续）：房主在联网大厅仍无独立「选关 UI」，当前 level 来源 = `UISetLayerb` 单机选关残留 + 冒烟
  `--level` 覆盖；C4 热更 CONFIG_PULL 接入仍未做（见 §4）。

## 3. 风险

- **R-A1 导出器需解析 TMX/plist**：现导出链只吃 TSV；TMX（XML）与 Cocos plist（XML）需新增解析器
  （`tools/config_exporter` 或独立小工具），且 TMX 对象层坐标/plist 数值语义需与 `exportCollisionToCore`
  （`FxFromFloat` 直拷、CCRect 闭区间）逐字段对齐，否则碰撞判定漂移。
- **R-A2 难度数组耦合**：`GameLevelData.h` 难度加成在运行时按 `m_nCurChooseDifficult` 读，导出时需按难度
  预生成 3 套 bin 或把难度系数纳入 bin 行，与内核 `Sp01Config` 的「本局常量」模型对齐。
- **R-A3 炮台渲染壳**：`Turret` 类作渲染壳需屏蔽其 CCAction/回调（同 `renderCoreSp01s` 的处理），且
  `paota_point` 的 `plist` 字段决定炮台贴图，需接入核心 `TurretState`。
- **R-A4 服务器无 TMX 解析**：S2/S4 的 bin 必须离线生成，服务器只读 bin，绝不在服务器端运行时解析 TMX。
- **R-A5 数据体积**：每关一个 bin 或多关卡合一个大 bin + 表内按关卡 id 索引（`WaveSpawnGroup`/`MapCollision`
  需加「关卡 id」维度）。推荐「一张大表按 levelId 索引」而非一关一 bin，减少加载次数。

## 4. 决策点（待评估）

- **bin 组织**：一关一 bin vs 全关卡一张大表（按 levelId 索引）。倾向大表（对齐 CConfigMgr「表名全局唯一」，
  一次加载全量，服务器/客户端各自持同一份）。
- **难度维度**：难度加成进 bin（预生成 3 套）vs 内核运行时乘难度系数。倾向进 bin（确定性更纯，内核零运行时表）。
- **顺序**：S1→S2→S3→S4→S5，每步绿了进下一步；S1 先做（波次通道最顺，快速打通「plist→bin→内核」范式）。
