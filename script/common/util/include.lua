--[[
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: include.lua
      Purpose:   common/util 模块统一加载入口

      清 package.loaded 缓存后依次 require log、timer、string_ext、number。
      加载后 xEngine.Log/Timer/StringExt/Number 可用。
]]

-- include.lua — Central require for common/util module
-- Clears package.loaded cache so re-execution works for hot-reload.
--
-- Usage: require("common.util.include")
-- After this, xEngine.Log / xEngine.Timer / xEngine.StringExt are available.

xEngine = xEngine or {}

package.loaded["common.util.log"] = nil
package.loaded["common.util.timer"] = nil
package.loaded["common.util.string_ext"] = nil
package.loaded["common.util.number"] = nil

require("common.util.log")
require("common.util.timer")
require("common.util.string_ext")
require("common.util.number")
