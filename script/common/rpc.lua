--[[
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: rpc.lua
      Purpose:   RPC 访问工具模块

      xEngine.RPC 封装 SWIG si_rpc_* 接口，提供 Send(target_actor_id, msg) 发送 RPC 消息。
]]

-- rpc.lua — xEngine.RPC global tool module
-- Wraps C++ RPC/gRPC operations exported via SWIG si_rpc_* functions.
--
-- Usage:
--   require("common.rpc")
--   xEngine.RPC.Send(target_actor_id, serialized_msg)

xEngine = xEngine or {}
xEngine.RPC = {}

function xEngine.RPC.Send(actor_id, msg)
    return luacore.si_rpc_send(actor_id, msg)
end
