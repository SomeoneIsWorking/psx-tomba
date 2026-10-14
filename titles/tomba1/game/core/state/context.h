// The one per-Core owner of Tomba! 1's native subsystem state.
#pragma once

#include "guest_task_slots.h"
#include "widescreen_projection.h"

class Core;

namespace tomba1 {

// One instance per Core, owned by `GameRuntime::createContext` and destroyed by its
// `destroyContext`. A picker runs Tomba! 1 beside Tomba! 2 in one process, so every piece of
// per-game native state lives here, reached by the frame driver and the native leaves alike.
struct Context {
  explicit Context(Core &core);

  GuestTaskSlots tasks;
  widescreen::ProjectionOwner widescreen;
};

Context &context(Core &core);
const Context &context(const Core &core);

} // namespace tomba1
