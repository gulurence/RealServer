-- include.lua — Central require for common/actor module
-- Clears package.loaded cache so re-execution works for hot-reload.
--
-- Usage: require("common.actor.include")
-- After this, xEngine.ActorBase, xEngine.ActorMgr, xEngine.ActorCreator are available.

xEngine = xEngine or {}

package.loaded["common.class"] = nil
package.loaded["common.actor.actor_base"] = nil
package.loaded["common.actor.actor_mgr"] = nil
package.loaded["common.actor.actor_creator"] = nil

require("common.class")
require("common.actor.actor_base")
require("common.actor.actor_mgr")
require("common.actor.actor_creator")
