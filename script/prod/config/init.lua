--[[
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: init.lua
      Purpose:   prod 环境 Config 进程启动脚本

      require common.include 与 actors.config.config，由 config.lua 尾部 registerPrototype/install 完成 ConfigClass 原型初始化，随后记录 ready 日志。
]]

-- init.lua — Config actor config (prod)
require("common.include")
require("actors.config.config")
xEngine.Log.info("[Config:prod] ready")
