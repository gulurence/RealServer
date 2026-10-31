--[[
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: init.lua
      Purpose:   prod 环境 Login 进程启动脚本

      require common.include 与 actors.login.login，由 login.lua 尾部 registerPrototype/install 完成 LoginClass 原型初始化，随后记录 ready 日志。
]]

-- init.lua — Login actor config (prod)
require("common.include")
require("actors.login.login")
xEngine.Log.info("[Login:prod] ready")
