--[[
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: config.lua
      Purpose:   配置热更 Actor 实现（单例，Global 进程）

      ConfigClass 经 si_config_load 加载 wave/map_collision/sp01 三份 RSCF bin 并聚合 contentHash，处理 GET_REQ 返回完整 bundle。
      GM reload 触发重载并广播 CONFIG_CHANGED_NTF，版本单调性由客户端共享 CConfigMgr 校验；任一 bin 缺失则 loaded=false 不部分提交。
]]

-- config.lua — Config actor implementation (单例, Global 进程, id=0)
--
-- 配置热更服务端: 加载 3 份 RSCF bin (wave/map_collision/sp01) → 聚合 contentHash
-- (rsconfig::CConfigMgr::AggregateHash), 客户端进大厅 CONFIG_GET_REQ 拉取完整 bundle;
-- 版本变更时 GM `reload` 广播 CONFIG_CHANGED_NTF 触发客户端重新拉取 (下一局生效)。
-- 版本单调性由共享 CConfigMgr 在客户端侧校验 (更小/同版本异内容拒绝)。

local AT = xEngine.ActorType

-- 配置模块消息 ID (镜像 config.proto ConfigMsgId)
local ConfigMsgId = {
    GET_REQ     = 1,
    GET_RSP     = 2,
    CHANGED_NTF = 3,
}

-- 单例内存态 (存于 xEngine.config_state, 跨热更保留)
local hot = xEngine.config_state or {}
xEngine.config_state = hot
if hot.version == nil then hot.version = 1 end
if hot.loaded == nil then hot.loaded = false end

-- 读版本 + 加载 bin (任一 bin 缺失/不可载则 loaded=false, 不部分提交)
local function load_config()
    local v = xEngine.Redis.Get("config:version")
    hot.version = tonumber(v) or 1
    hot.loaded = luacore_config.si_config_load("shared/config/data")
end

local ConfigClass = xEngine.Class.define("Config", xEngine.ActorBase)

function ConfigClass:onInstall()
    load_config()
    xEngine.Log.info("[Config] onInstall version=%d loaded=%s", hot.version, tostring(hot.loaded))
    return true
end

function ConfigClass:onUnInstall()
    xEngine.Log.info("[Config] onUnInstall")
    return true
end

function ConfigClass:data_load(actor_data) return true end
function ConfigClass:data_save(save_type) return true end
function ConfigClass:onInit()    return true end
function ConfigClass:onCreate()  return true end

--------------------------------------------------------------------------------
-- 业务消息总入口
--------------------------------------------------------------------------------
function ConfigClass:on_rpc_forward(wrapped)
    local payload   = luacore.si_net_body_payload(wrapped)
    local smallType = luacore.si_pb_small_type(payload)

    if smallType == ConfigMsgId.GET_REQ then
        if not hot.loaded then
            xEngine.Log.error("[Config] get: config not loaded, serve empty")
            return ""
        end
        xEngine.Log.info("[Config] get: serve version=%d", hot.version)
        return luacore_config.si_config_get_rsp(hot.version)
    end

    xEngine.Log.warn("[Config] unknown small_type=%d", smallType)
    return ""
end

--------------------------------------------------------------------------------
-- GM 命令 (WebPlatform GM 控制台经 `actor 105 0 <cmd>` 触发)
--------------------------------------------------------------------------------
function ConfigClass:on_gm_cmd(cmdline)
    if cmdline == "status" or cmdline == "" then
        return string.format("config version=%d loaded=%s", hot.version, tostring(hot.loaded))
    elseif cmdline == "reload" then
        load_config()
        if hot.loaded then
            -- 广播版本变更: playerId==0 哨兵 = 全员广播 (见 gate.lua on_rpc_push),
            -- 客户端收到 CONFIG_CHANGED_NTF 后据版本比对决定是否重新拉取 (下一局生效)。
            luacore.si_push_rpc(0, luacore_config.si_config_changed_ntf(hot.version))
            return string.format("config reloaded version=%d (broadcast sent)", hot.version)
        end
        return "config reload failed (bin load error)"
    end
    return string.format("config: unknown gm cmd '%s' (try 'status'/'reload')", tostring(cmdline))
end

xEngine.ActorMgr.registerPrototype(AT.CONFIG, ConfigClass)
xEngine.ActorMgr.install(AT.CONFIG, 0)
