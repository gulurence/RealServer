# 关卡数据 bin（C6 关卡数据授权产物）

本目录是 C6「关卡数据授权」（`docs/c6-level-data-authoring.md`）的离线产物：把客户端
Cocos plist/TMX 离线导出成内核/框架可直接消费的 RSCFG bin。服务器只读 bin（R-A4，绝不在
服务器端运行时解析 TMX）。

## 产物（运行时直接消费）

| 文件 | 表 | 内容 | 行数 |
|---|---|---|---|
| `wave.bin` | `wave` + `wave_spawn` | 波次计划（含 `pt_N`→`kClassTurret` 哨兵组） | 199 + 1470 |
| `sp01.bin` | `sp01` + `turret` | 小怪属性 + 炮台属性（`ptinfo_0..5`） | 120 + 6 |
| `map_collision.bin` | `map` + `map_rect` + `map_entry` + `turret_point` | 地图碰撞 + 出生点 + 炮台点（`paota_point`） | 48 + 1877 + 217 + 87 |

关卡编号 11..68（48 关，`bb`=大关、`ss`=小关）。`turret_point` 87 行覆盖 17 个含
`paota_point` 对象层的关卡（12,13,15,18,22,25,27,31,33,36,38,42,44,47,62,65,68）。

## 源数据（重新授权用）

`source/` 是从客户端原始资源包 `client/soulofgun/samples/HelloCpp/Resources/res`
（3.3MB zip，2015-02-04）解出的源文件：

- `source/map/map/level{11..68}.tmx` — 48 关地图（含对象层 `paota_point`/`people`/`bullet`/`entry`）
- `source/out/out{11..68}.plist` — 48 关波次
- `source/plist/enemyInfo.plist` — 小怪 + 炮台属性（`ptinfo_0..5`）

## 重新授权命令（数据变更后重跑）

```bash
tools/sp01_exporter/sp01_exporter.exe shared/config/data/sp01.bin          shared/config/data/source/plist/enemyInfo.plist
tools/tmx_exporter/tmx_exporter.exe  shared/config/data/map_collision.bin  shared/config/data/source/map/map/level*.tmx
tools/wave_exporter/wave_exporter.exe shared/config/data/wave.bin          shared/config/data/source/out/out*.plist
```

（`wave_exporter`/`sp01_exporter`/`tmx_exporter` 用各自的 `build_*.bat` 重编译。）

## 消费（读 bin → 注入核心）

读 bin 用 `shared/config/CBinReader.h` + 强类型表 `shared/config/{CWaveTable,CMapTable,
CSp01Table,CTurretTable}.h`：

- 波次：`CWaveTable::GetWavePlan(levelId)`
- 碰撞：`CMapTable::GetMapCollision(levelId)`
- 小怪：`CSp01Table::BuildConfigMap(difficulty)`
- 炮台：`CTurretTable::BuildTurretConfigMap()` + `CTurretTable::GetTurretSpawns(levelId)`

注入核心走 `CBattleCore::SetWavePlan / SetSp01Config / SetCollision / SetTurretSpawns /
SetTurretConfig`（`shared/battlecore/BattleCore.h`）。
