#include "context.h"

#include "core.h"
#include "game.h"

#include <cstdlib>
#include <lucent/log.h>

namespace tomba1 {

Context::Context(Core &) {}

Context &context(Core &core) {
  if (!core.game || !core.game->sessionState) {
    lucent::error("tomba1-core", "native owner state is absent; the runtime never created this Core's context");
    std::abort();
  }
  return static_cast<Context &>(*core.game->sessionState);
}

const Context &context(const Core &core) {
  return context(const_cast<Core &>(core));
}

} // namespace tomba1
