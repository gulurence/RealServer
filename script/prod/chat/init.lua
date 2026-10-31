--[[
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: init.lua
      Purpose:   prod 环境 Chat 进程启动脚本

      require common.include 与 actors.chat.chat，由 chat.lua 尾部 registerPrototype/install 完成 ChatClass 原型初始化，随后记录 ready 日志。
]]

-- init.lua — Chat actor config (prod)
require("common.include")
require("actors.chat.chat")
xEngine.Log.info("[Chat:prod] ready")
