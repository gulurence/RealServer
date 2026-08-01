// CActor.i -- Actor base class export for Lua
// Enables cross-language polymorphism via SWIG directors.
// Lua tables can override CActor's pure virtual methods.

%include "xActor/ActorDefine.h"
%include "xActor/Actor.h"

// Enable SWIG directors for CActor:
// When a Lua table "subclasses" CActor, C++ virtual calls are
// forwarded to the Lua table. This is the core bridge that allows
// Actor business logic to be implemented in Lua.
%feature("director") CActor;

// CActor pure virtual methods that Lua will override:
//   virtual bool Init() = 0
//   virtual bool Install() = 0
//   virtual bool UnInstall() = 0
//   virtual CActor* Create(ActorID) = 0
