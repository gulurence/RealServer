--[[
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: forward.lua
      Purpose:   Gate 的 RPC 转发处理器

      为 GateClass 定义 on_rpc_forward 占位实现，处理其它进程转发到 Gate 的消息。
      当前仅记录日志，TODO 为转发到目标客户端连接。
]]

-- forward.lua — Handle RPC forwarded from other processes
local GateClass = xEngine.Gate

function GateClass:on_rpc_forward(msg)
    xEngine.Log.info("[Gate] on_rpc_forward")
    -- TODO: forward message to target client connection
end
