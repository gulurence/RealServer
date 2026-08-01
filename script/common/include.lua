-- include.lua — Top-level common module entry point
-- Requires all common sub-modules in dependency order.
-- Re-executing this file performs a hot-reload of all common modules.
--
-- Usage: require("common.include")
-- After this, all xEngine.* modules are available:
--   xEngine.Log / xEngine.Timer / xEngine.StringExt
--   xEngine.Class / xEngine.ActorBase / xEngine.ActorMgr
--   xEngine.Redis / xEngine.DB / xEngine.RPC

xEngine = xEngine or {}

package.loaded["common.actor_type"] = nil
package.loaded["common.class"] = nil
package.loaded["common.redis"] = nil
package.loaded["common.db"] = nil
package.loaded["common.rpc"] = nil
package.loaded["common.util.include"] = nil
package.loaded["common.actor.include"] = nil

require("common.actor_type")
require("common.class")
require("common.redis")
require("common.db")
require("common.rpc")
require("common.util.include")
require("common.actor.include")
