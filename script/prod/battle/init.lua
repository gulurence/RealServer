--[[
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: init.lua
      Purpose:   prod 环境 Battle 进程启动脚本

      require common.include 与 actors.battle.battle，由 battle.lua 尾部 registerPrototype/install 完成 BattleClass 原型初始化，随后记录 ready 日志。
]]

-- init.lua — Battle actor config (prod)
require("common.include")
require("actors.battle.battle")
xEngine.Log.info("[Battle:prod] ready")
