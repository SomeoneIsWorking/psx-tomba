// The one per-Core owner of Tomba! 1's native subsystem state.
#pragma once

#include "widescreen_projection.h"

class Core;

namespace tomba1 {

// One instance per Core, owned by `GameRuntime::createContext` and destroyed by its
// `destroyContext`. It exists because the guest-widescreen plan is per-GAME state that a second
// Core in one process would otherwise share, and because the frame driver and the two projection
// override sites must reach the SAME owner instance rather than each keeping a copy.
struct Context {
  explicit Context(Core &core);

  widescreen::ProjectionOwner widescreen;
};

Context &context(Core &core);
const Context &context(const Core &core);

} // namespace tomba1
