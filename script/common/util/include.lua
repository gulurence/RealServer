-- include.lua — Central require for common/util module
-- Clears package.loaded cache so re-execution works for hot-reload.
--
-- Usage: require("common.util.include")
-- After this, xEngine.Log / xEngine.Timer / xEngine.StringExt are available.

xEngine = xEngine or {}

package.loaded["common.util.log"] = nil
package.loaded["common.util.timer"] = nil
package.loaded["common.util.string_ext"] = nil

require("common.util.log")
require("common.util.timer")
require("common.util.string_ext")
