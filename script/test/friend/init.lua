--[[
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: init.lua
      Purpose:   test 环境 Friend 进程启动脚本

      require common.include 与 actors.friend.friend，由 friend.lua 尾部 registerPrototype/install 完成 FriendClass 原型初始化，随后记录 ready 日志。
]]

-- init.lua — Friend actor config (test)
require("common.include")
require("actors.friend.friend")
xEngine.Log.info("[Friend:test] ready")
