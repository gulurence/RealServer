-- include.lua — Top-level common module entry point
-- Requires all common sub-modules in dependency order.
-- Re-executing this file performs a hot-reload of all common modules.
--
-- Usage: require("common.include")
-- After this, all xEngine.* modules are available:
--   xEngine.Log / xEngine.Timer / xEngine.StringExt
--   xEngine.Class / xEngine.ActorBase / xEngine.ActorMgr
--   xEngine.Redis / xEngine.DB / xEngine.RPC

xEngine = xEngine or {}

-- 幂等 require: 不再清 package.loaded。否则热更链路 (LuaHotFix 重跑业务 chunk 时其内部
-- require("common.include")) 会重建 common.class 等框架模块的 file-local 注册表
-- (ClassRegistry / protoRegistry / localInstances), 导致既有实例失去类表引用而断链。
-- 标准 require 尊重 package.loaded 缓存, 框架模块首次加载后即常驻。
require("common.actor_type")
require("common.msg_id")
require("common.class")
require("common.redis")
require("common.db")
require("common.rpc")
require("common.account")
require("common.util.include")
require("common.actor.include")
require("common.migration")

-- LuaHotFix 热更插件 (全局公共模块): xEngine.HotFix
xEngine.HotFix = require("common.hotfix")
