// ActorCreator.i -- Actor factory singleton export for Lua
// ActorCreator uses CRTP xSingleton<T> which SWIG cannot directly wrap.
// We provide Lua-friendly static helper functions via %extend.

%include "xActor/ActorCreator.h"

// Extend ActorCreator with Lua-callable static methods.
// These bypass the xSingleton CRTP pattern that SWIG cannot parse.
%extend ActorCreator {
    static bool RegistActor(CActor* actor) {
        return ActorCreator::RegistActor(actor);
    }
    static CActor* CreateActor(ActorType type, ActorID id) {
        return ActorCreator::Create(type, id);
    }
    static void SetCreateEnableProxy(ActorType type, bool enable) {
        ActorCreator::SetCreateEnable(type, enable);
    }
}
