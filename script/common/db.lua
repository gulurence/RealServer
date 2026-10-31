--[[
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: db.lua
      Purpose:   数据库访问工具模块

      xEngine.DB 封装 SWIG si_db_* 接口，提供 Select/Insert/Update/Delete 四个数据库操作。
]]

-- db.lua — xEngine.DB global tool module
-- Wraps C++ Database operations exported via SWIG si_db_* functions.
--
-- Usage:
--   require("common.db")
--   local rows = xEngine.DB.Select("players", "id=100")
--   xEngine.DB.Insert("players", data)
--   xEngine.DB.Update("players", data, "id=100")
--   xEngine.DB.Delete("players", "id=100")

xEngine = xEngine or {}
xEngine.DB = {}

function xEngine.DB.Select(table_name, where)
    return luacore.si_db_select(table_name, where)
end

function xEngine.DB.Insert(table_name, data)
    return luacore.si_db_insert(table_name, data)
end

function xEngine.DB.Update(table_name, data, where)
    return luacore.si_db_update(table_name, data, where)
end

function xEngine.DB.Delete(table_name, where)
    return luacore.si_db_delete(table_name, where)
end
