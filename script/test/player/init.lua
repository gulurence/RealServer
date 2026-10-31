--[[
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: init.lua
      Purpose:   test 环境 Player 进程启动脚本

      require common.include 与 actors.player.player，由 player.lua 尾部 registerPrototype/install 完成 PlayerClass 原型初始化，随后记录 ready 日志。
]]

-- init.lua — Player actor config (test)
require("common.include")
require("actors.player.player")
xEngine.Log.info("[Player:test] ready")
