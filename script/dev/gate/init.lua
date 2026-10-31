--[[
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: init.lua
      Purpose:   dev 环境 Gate 进程启动脚本

      require common.include、actors.gate.gate 与 actors.gate.on_rpc.include，由 gate.lua 尾部 registerPrototype/install 完成 GateClass 原型初始化，随后记录 ready 日志。
]]

-- init.lua — Gate actor config (dev)
require("common.include")
require("actors.gate.gate")
require("actors.gate.on_rpc.include")
xEngine.Log.info("[Gate:dev] ready")
