--[[
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: migration.lua
      Purpose:   灰度迁移「先迁后杀」模块

      xEngine.ActorMigration 提供 saveAll（旧进程落 Redis + 驱逐注册）与 run（新进程扫描 actor:index 重建孤儿 Actor）。
      Player/Battle 经 C++ CActorMgr 路径定位 delegate 实例恢复，Spectator 无恢复态只清残留索引，恢复完成后写 cluster:migrate:restored 进度键。
]]

-- migration.lua — 灰度迁移「先迁后杀」: 保存当前负载 Actor 到 Redis, 新进程恢复重建。
-- Registered as xEngine.ActorMigration.
--
-- 语义:
--   saveAll()  — 旧进程「迁」: 枚举本地 Actor → data_save(2) 落 Redis → 驱逐注册
--                (DEL cluster:actor:{type}:{id}, 保留 actor:index:{type} 供恢复枚举)。
--                由 GmCommandMgr 的 migrate_save 命令经 DispatchSync 调用。
--   run()      — 新进程「杀后恢复」: 扫描 actor:index:{type}, 对已驱逐(无 cluster:actor
--                注册)且非原型(id≠0)的孤儿 Actor 经 C++ 路径重建 → data_load 从 Redis
--                恢复 → Install 触发 RegisterActor 更新路由 (cluster:actor pid→新)。
--                由 ProcessEnv 启动阶段在调度器就绪后经 DispatchSync 调用一次。
--
-- 数据源分工 (与 GmCommandMgr.cpp 注释一致):
--   cluster:actor:{type}:{id}   有 TTL(30s, 心跳续期), 进程死后过期 → 存活租约信号
--   cluster:actor:by_type:{type} 有 TTL, 同上
--   actor:index:{type}          无 TTL, 仅 DeregisterActor 移除 → 持久恢复枚举源
--
-- 关键坑 (Player/Battle 双实例):
--   Player/Battle 经 RPC 路径创建 (ActorCreator::Create → CLuaActor::Create), 其权威
--   运行态挂在 delegate 表 (instance B), 不在 Lua localInstances 里。故迁移必须走 C++
--   CActorMgr 定位 delegate (si_actor_mgr_local_ids / si_actor_data_save / si_actor_data_load),
--   不能用 ActorMgr.forEachInstance / createLocal / findLocal —— 否则会漏掉 RPC 创建的
--   Actor (saveAll 记 0) 或把 data_load 灌进幻影实例 A (真正收消息的 B 拿不到恢复态)。
--
-- 只有「多实例且状态可落 Redis」的 Actor 会被真正恢复 (Player/Battle); 单例(id=0)
-- 由各自 init.lua 的 install(type,0) 自行重建, run() 跳过 id=0。

xEngine = xEngine or {}
xEngine.ActorMigration = {}

-- Lua 5.4 numbers are floats by default; SWIG expects integer subtype (同 actor_mgr.lua)。
local i32 = luacore.toint32
local i64 = luacore.toint64

-- 全部 Actor 类型 (镜像 xEngine.ActorType, 供 run() 遍历 canHostType)
local ACTOR_TYPES = { 100, 101, 102, 103, 104, 105, 201, 203, 204 }

-- SMEMBERS 由 C++ join 成逗号串 (成员均为整数 id 不含逗号), 拆成表
local function split_csv(csv)
    local out = {}
    for v in (csv or ""):gmatch("[^,]+") do
        if v ~= "" then out[#out + 1] = v end
    end
    return out
end

--------------------------------------------------------------------------------
-- saveAll — 旧进程「迁」: 本地 Actor 内存态落 Redis + 驱逐注册 (保留 actor:index)
--------------------------------------------------------------------------------
function xEngine.ActorMigration.saveAll()
    local count = 0
    for _, actorType in ipairs(ACTOR_TYPES) do
        -- 枚举走 C++ CActorMgr 权威索引 (含 RPC 创建的 Player/Battle delegate 实例)。
        local ids = split_csv(luacore.si_actor_mgr_local_ids(i32(actorType)))
        for _, idStr in ipairs(ids) do
            local id = tonumber(idStr)
            if id and id ~= 0 then
                -- data_save(2) = 迁移保存 (Battle 序列化核心/元数据到 battle:migrate:*,
                -- 其余类型无状态返回 true; Player 已是 Redis 权威, 无需额外保存)。
                if not luacore.si_actor_data_save(i32(actorType), i64(id), 2) then
                    xEngine.Log.error("[Migration] saveAll: data_save failed type=%d id=%s", actorType, idStr)
                end

                -- 驱逐注册: 移除存活租约 (cluster:actor) 但保留 actor:index, 使新进程 run()
                -- 能立即识别其为孤儿并重建 (无需等 30s TTL 过期)。心跳仅 Expire 不会重建键,
                -- 故驱逐后旧进程在 kill 前的短暂窗口内不会复活该注册。
                xEngine.Redis.Del("cluster:actor:" .. actorType .. ":" .. idStr)
                xEngine.Redis.SRem("cluster:actor:by_type:" .. actorType, idStr)

                -- Player 迁出: 计数器先减 (新进程 install 时再 Incr), 否则灰度多次滚动
                -- cluster:load:player_count 累积虚高, 登录排队限流误判 (实测滚多轮后 34 vs 实际 4)。
                if actorType == 201 then
                    xEngine.Redis.Decr("cluster:load:player_count")
                end
                count = count + 1
            end
        end
    end
    xEngine.Log.info("[Migration] saveAll: %d actor(s) flushed+evicted", count)
    return true
end

--------------------------------------------------------------------------------
-- run — 新进程「杀后恢复」: 重建孤儿 Actor (data_load 恢复 + RegisterActor 更新路由)
--------------------------------------------------------------------------------
-- 单个孤儿重建: 满足「可承载 + id≠0 + 无存活租约 + 本地未持有」才重建。
-- 返回是否重建 (重建失败告警但不抛错, 避免阻断其它类型恢复)。
local function recover_one(actorType, id)
    local idStr = xEngine.Number.intKey(id)

    -- 孤儿判定: cluster:actor:{type}:{id} 的 pid 字段为空 ⇒ 已被 saveAll 驱逐或
    -- 原进程死后 TTL 过期 ⇒ 无人承载。若仍存在(他进程存活承载)则跳过, 避免抢占。
    -- 注意: si_redis_hget miss 返回 "" 而非 nil, 故判空串 (旧代码判 ~= nil 恒真,
    -- 导致恢复永不触发, 已修)。
    local owner = xEngine.Redis.HGet("cluster:actor:" .. actorType .. ":" .. idStr, "pid")
    if owner ~= nil and owner ~= "" then
        return false
    end
    if luacore.si_actor_mgr_find(i32(actorType), i64(id)) then
        return false
    end

    -- 经 C++ 路径重建 (与 ActorRpcHandlers 的 CreateActor 同构: Create → AddInstance →
    -- Install), 而非 createLocal —— 后者只把 data_load 灌进 localInstances 的幻影实例 A,
    -- 真正收消息的 delegate 实例 B 拿不到恢复态 (Battle 会丢 core/frame 状态)。
    local cpp = luacore.si_actor_creator_create(i32(actorType), i64(id))
    if not cpp then
        xEngine.Log.error("[Migration] run: create failed type=%d id=%s", actorType, idStr)
        return false
    end
    if not luacore.si_actor_mgr_add_instance(cpp) then
        xEngine.Log.error("[Migration] run: add_instance failed type=%d id=%s", actorType, idStr)
        return false
    end
    if not luacore.si_actor_mgr_install(i32(actorType), i64(id)) then
        luacore.si_actor_mgr_remove_instance(i32(actorType), i64(id))
        xEngine.Log.error("[Migration] run: install failed type=%d id=%s", actorType, idStr)
        return false
    end

    -- 恢复 delegate 实例 B 的内存态 (Battle 从 battle:migrate:core 反序列化核心; Player
    -- 读回 player:equip)。Install 已触发 RegisterActor 更新 cluster:actor 路由 (pid→新)。
    luacore.si_actor_data_load(i32(actorType), i64(id))
    return true
end

-- 残留观战注册清理: Spectator (204) 是纯派生对象 (data_save/data_load 空、无恢复态),
-- 不应冷恢复 —— 崩溃/重启后随下一次开战 (room.lua handle_start) 与 BattleActor 一一对应
-- 地重建。这里在冷恢复阶段顺带清理其持久索引 actor:index:204 中的孤儿条目, 否则下次重启
-- 反复枚举, WebPlatform「Actor 实例」页残留已结束战斗的观战 Actor (历史 eager 创建 +
-- delete 路由 bug 遗留)。
-- 判定「残留」: 战斗已结束 (无 battle:roster) 或 存活租约已蒸发 (cluster:actor:204
-- 无 pid, 进程死后 30s TTL 过期)。两者同时不满足 (战斗仍存活且他进程仍在承载) 才保留。
local function cleanup_spectator(idStr)
    local battleLive = xEngine.Redis.Get("battle:roster:" .. idStr) or ""
    local owner = xEngine.Redis.HGet("cluster:actor:204:" .. idStr, "pid") or ""
    if battleLive ~= "" and owner ~= "" then
        return false
    end
    xEngine.Redis.Del("cluster:actor:204:" .. idStr)
    xEngine.Redis.SRem("cluster:actor:by_type:204", idStr)
    xEngine.Redis.SRem("actor:index:204", idStr)
    return true
end

function xEngine.ActorMigration.run()
    -- 排队恢复: 先按 (类型, id) 顺序收集全部孤儿, 再逐个 rebuild (recover_one 已含
    -- 容错隔离, 单个失败不阻断后续)。顺序固定 => 恢复可预测, 供 gray_roll.ps1 轮询
    -- cluster:migrate:restored:<processId> 得知「上一批已恢复完」再滚下一批 (排队)。
    local orphans = {}
    local cleanedSpectators = 0
    for _, actorType in ipairs(ACTOR_TYPES) do
        if xEngine.ActorMgr.canHostType(actorType) then
            local ids = split_csv(xEngine.Redis.SMembers("actor:index:" .. actorType))
            for _, idStr in ipairs(ids) do
                local id = tonumber(idStr)
                if id and id ~= 0 then   -- 跳过原型/单例 (id=0)
                    if actorType == 204 then
                        -- Spectator 无恢复态, 不进入 orphan 重建队列, 只清残留索引
                        if cleanup_spectator(idStr) then
                            cleanedSpectators = cleanedSpectators + 1
                        end
                    else
                        orphans[#orphans + 1] = { actorType, id }
                    end
                end
            end
        end
    end

    local recovered = 0
    for _, o in ipairs(orphans) do
        if recover_one(o[1], o[2]) then
            recovered = recovered + 1
        end
    end

    -- 恢复完成信号: 写本进程进度键, gray_roll.ps1 据此等待本批恢复完再滚下一批。
    -- si_get_process_id() = 集群 processId (CProcessManager 启动时分配, 与 RPC 目标一致)。
    local selfPid = luacore.si_get_process_id()
    if selfPid and selfPid ~= 0 then
        xEngine.Redis.Set("cluster:migrate:restored:" .. xEngine.Number.intKey(selfPid),
            tostring(recovered))
    end

    xEngine.Log.info("[Migration] run: recovered %d/%d actor(s), cleaned_spectators=%d, selfPid=%s",
        recovered, #orphans, cleanedSpectators, tostring(selfPid))
    return recovered
end

return xEngine.ActorMigration
