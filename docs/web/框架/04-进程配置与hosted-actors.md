# 进程配置与 hosted-actors

> 本文记录「进程类型 → 可承载 Actor 类型列表」从硬编码 switch 抽成 XML 配置的方案（M0 基础设施）。
> 核心结论：映射关系放入 `config/server.xml` 的 `<hosted-actors>` 段，启动时读入；
> 配置缺失或某类型未声明时**回退到硬编码** `xProcessDefine.h` 的 `GetHostedActorTypes`。
> 配置读取层本阶段只负责「解析 + 提供查询接口」，**不接线**——最终 `switch → config` 的接线待
> `xProcessDefine.h` 枚举扩展（另一 agent）合并后再接。

---

## 1. 背景与目标

进程类型（`EProcessType`）决定本进程可负载哪些 Actor 类型，当前映射硬编码在
`engine/xCluster/xProcessDefine.h` 的 `GetHostedActorTypes`（`switch`）。

问题：

- 每新增一种进程 / Actor 都要改 C++ 并重编译；
- 部署时想临时调整某进程承载范围（如让 Game 进程不再负载 World）只能改代码。

目标：把该映射抽到 `config/server.xml`，运行时读入；同时**保留硬编码作为回退**，保证
「配置缺失 / 解析失败 / 枚举尚未扩展」时进程仍能按既有 switch 启动。

---

## 2. XML schema

新增一段顶层 `<hosted-actors>`（`<server>` 的直接子节点），声明全量映射表：

```xml
<server>
    <process type="100" name="gate-01" host="0.0.0.0"/>

    <hosted-actors>
        <process type="100">
            <actor type="100"/>   <!-- Gate -->
        </process>
        <process type="101">
            <actor type="101"/>   <!-- Login -->
        </process>
        <process type="102">
            <actor type="102"/>   <!-- Friend -->
            <actor type="104"/>   <!-- Chat -->
            <actor type="105"/>   <!-- Config -->
        </process>
        <process type="201">
            <actor type="201"/>   <!-- Player -->
            <actor type="103"/>   <!-- Room -->
        </process>
        <process type="202">
            <actor type="203"/>   <!-- Battle -->
            <actor type="204"/>   <!-- Spectator -->
        </process>
    </hosted-actors>
</server>
```

结构约定：

| 元素 | 属性 | 说明 |
|---|---|---|
| `<hosted-actors>` | — | 声明表根，可含多个 `<process>` |
| `<process>` | `type` | 进程类型值（`EProcessType`，如 100/101/102/201/202） |
| `<actor>` | `type` | 该进程可承载的 Actor 类型值（`EActorType`），可多个 |

> 类型值沿用「值域重叠、语义不同」约定：`102` 在进程层是 Global、在 Actor 层是 Friend，
> 靠 `type` 所在的上下文区分（`<process type>` 是进程、`<actor type>` 是 Actor）。

**另一种等价写法（嵌套形式，可选）**：也可直接在身份段 `<process>` 内嵌 `<hosted-actors>`，
只声明「本进程」可承载的 Actor 类型：

```xml
<process type="201" name="game-01" host="0.0.0.0">
    <hosted-actors>
        <actor type="201"/>
        <actor type="202"/>
        <actor type="203"/>
        <actor type="204"/>
    </hosted-actors>
</process>
```

两种写法都受支持，优先级见 §4。示例模板 `config/server.xml.example` 用**顶层声明表**，
因为它是「进程类型 → 列表」的全量映射，单一文件即可覆盖所有进程类型。

---

## 3. 解析流程

`CServerConfig::Load()`（`engine/xConfig/xServerConfig.cpp`）的解析顺序：

```
parseDoc(server.xml)
  └─ ParseServerSection      <platform> <grpc>
  └─ ParseProcessSection     <process> — 同时解析嵌套 <hosted-actors> (写入 process.hostedActors)
  └─ ParseHostedActorsSection <hosted-actors> — 顶层声明表 (写入 ServerXmlConfig.hostedActors)
  └─ ParseClusterSection     <cluster>
  └─ ParseRedisSection       <redis>
  └─ ParseDBSection          <db>
```

`ParseHostedActorsSection` 遍历 `<hosted-actors><process>`，对每个 `process` 再遍历其 `<actor>` 子节点，
把 `type` 值（`safe_stoi`，非法/为 0 跳过）收集进 `HostedActorsConfig{processType, actorTypes}` 列表。

关键结构（`engine/xConfig/xServerConfig.h`）：

```cpp
struct HostedActorsConfig {
    int32                 processType = 0;   // EProcessType
    std::vector<ActorType> actorTypes;       // 该进程可承载的 Actor 类型列表
};

struct ServerXmlConfig {
    ProcessXmlConfig process;                       // 身份段, 含嵌套 hostedActors
    std::vector<HostedActorsConfig> hostedActors;   // 顶层声明表
    ...
};
```

---

## 4. 默认值 / 回退策略

查询入口：

```cpp
// 查询任意进程类型
std::vector<ActorType> CServerConfig::GetHostedActorTypes(EProcessType type) const;
// 查询本进程
std::vector<ActorType> CServerConfig::GetHostedActorTypes() const;
```

优先级（`GetHostedActorTypes(EProcessType)` 内实现）：

1. **本进程嵌套声明**：`<process><hosted-actors>` 已解析且非空，且查询类型 == `process.type` → 返回嵌套列表。
2. **顶层声明表**：`ServerXmlConfig.hostedActors` 中 `processType` 匹配且 `actorTypes` 非空 → 返回该列表。
3. **硬编码回退**：`::GetHostedActorTypes(type)`（`xProcessDefine.h` 的 switch）。

回退规则：

- `<hosted-actors>` 整段缺失 → 全部进程类型走硬编码（等价于旧行为）。
- 某 `<process>` 未声明或 `actorTypes` 为空 → 该类型走硬编码。
- `<actor type>` 解析为 0 或非数字 → 该条忽略（不计入列表）。
- XML 解析失败（`parseDoc` 返回 false）→ `Load()` 提前返回 false，`main.cpp` 打 XERR 后
  继续用默认 `ServerXmlConfig`（`hostedActors` 为空），所有查询回退硬编码。

> 语义约定：**配置里空列表视为「未配置」而非「显式空」**，因此空列表同样回退硬编码。
> 这是为了安全——写漏 `<actor>` 不致于让进程「什么都不负载」。

---

## 5. 与硬编码 switch 的关系

| 位置 | 职责 | 本阶段改动 |
|---|---|---|
| `xProcessDefine.h` `GetHostedActorTypes` | 硬编码默认映射（回退基准） | **不动**（枚举扩展归另一 agent） |
| `xServerConfig` | 读配置 + 覆盖接口 | 本 agent：解析 + `GetHostedActorTypes` 查询 |
| `CActorLoadManager::Init` / `ProcessEnv` | 消费方（调用 `GetHostedActorTypes(m_eType)`） | **不接线**（待合并） |

配置是「覆盖层」，硬编码是「底座」。两者并存期间行为一致（示例模板里的映射 == 硬编码 switch），
不一致时以配置为准（仅当查询类型在配置中有非空声明时）。

---

## 6. 合并后的接线方案（待另一 agent 合并后执行）

合并目标是让消费方从「直接调硬编码」切到「经 `CServerConfig` 查询」。改点（非本 agent 范围，记录备查）：

1. `engine/xCluster/CActorLoadManager.cpp:17`：
   `auto hosted = GetHostedActorTypes(selfType);` → `auto hosted = CServerConfig::instance()->GetHostedActorTypes(selfType);`
2. `engine/xEnv/ProcessEnv.cpp:180`：
   `LuaVM::instance()->LoadAllActors(cfg.platform, GetHostedActorTypes(m_eType))` →
   `... CServerConfig::instance()->GetHostedActorTypes(m_eType)`。

> 注意：`GetHostedActorTypes` 是成员函数后，与全局 `::GetHostedActorTypes` 同名；
> 已改为经 `CServerConfig::instance()` 调用，避免歧义。`ActorTypeToProcessType`（Actor → 进程反向映射）
> 不在本次配置化范围内，仍走硬编码，后续如需配置化再单独决策。

---

## 潜在问题

> 本文潜在问题已纳入总登记表 [潜在问题](../潜在问题.md)，由 `tools/gen-problems.js` 从 `problems.json` 自动生成。

<!-- PROBLEMS:START -->
| ID | 状态 | 优先级 | 归期 | 问题 | 处理/方向 |
|---|---|---|---|---|---|
| **D1** | ⚠️ 已缓解 | 中 | M0 | XML 解析失败静默回落 Gate | 已缓解：Load 失败 XERR 带「将以 DEFAULT identity (type=%d) 启动」提示；根治方向=fail-fast 或回落非 Gate，仍需人工核对 Loaded 日志。 |
| **D2** | ⚠️ 已缓解 | 中 | M0 | 配置与硬编码不一致 | 已缓解：GetHostedActorTypes 回退硬编码打 XWRN（含 type）；更进一步「启动期打印 config 命中 vs fallback 各自类型」校验日志仍可选。 |
| **D3** | ⚠️ 已缓解 | 中 | M0 | type 值域合法性 | 部分处理：<actor type> 已加 ActorTypeToProcessType != EProcessType_None 校验（嵌套+顶层），非法值告警忽略；<process type>（entry.processType/process.type）仍只判非 0。 |
| **D4** | 🔴 仍开放 | 低 | — | 启动顺序依赖（查询须在 Load 之后） | GetHostedActorTypes 依赖 Load 完成，消费方在 OnActorInit（更后）调用，顺序成立；若提前到 Load 前会静默拿空配置→全量回退硬编码且无告警。 |
| **D5** | 🔴 仍开放 | 低 | — | 空列表语义（无法表达显式不承载） | 空 <actor> 列表被当「未配置」回退硬编码而非「显式不承载」；若确实需要某进程什么都不负载，需引入显式占位（如 none）。 |
| **D6** | 🔴 仍开放 | 低 | — | 顶层表 vs 嵌套声明冲突无告警 | 同进程类型在顶层表和 <process> 嵌套都声明且不一致时，嵌套优先且无冲突告警；示例模板只用顶层表避免歧义。 |
| **D7** | 🔴 仍开放 | 中 | — | 同类型多实例进程不支持 | hosted-actors 按「进程类型」声明，天然不支持同类型不同承载（如两个 Game 一个只跑 Battle 一个只跑 Player）；若需按进程细分承载得把粒度下沉到进程实例。 |
<!-- PROBLEMS:END -->
