#include "context.h"

#include "core.h"

#include <cstdlib>
#include <lucent/log.h>

namespace tomba1 {

Context::Context(Core &) {}

Context &context(Core &core) {
  Context *owned = static_cast<Context *>(core.gameCtx);
  if (!owned) {
    lucent::error("tomba1-core", "native owner state is absent; the runtime never created this Core's context");
    std::abort();
  }
  return *owned;
}

const Context &context(const Core &core) {
  return context(const_cast<Core &>(core));
}

} // namespace tomba1
