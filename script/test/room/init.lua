--[[
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: init.lua
      Purpose:   test 环境 Room 进程启动脚本

      require common.include 与 actors.room.room，由 room.lua 尾部 registerPrototype/install 完成 RoomClass 原型初始化，随后记录 ready 日志。
]]

-- init.lua — Room actor config (test)
require("common.include")
require("actors.room.room")
xEngine.Log.info("[Room:test] ready")
