--[[
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: include.lua
      Purpose:   Gate RPC 处理器统一加载入口

      require actors.gate.on_rpc.forward，集中加载 Gate 的 RPC 处理器。
      由各平台 gate/init.lua 在加载 gate.lua 后引用。
]]

-- on_rpc/include.lua — Gate RPC handlers (shared)
require("actors.gate.on_rpc.forward")
