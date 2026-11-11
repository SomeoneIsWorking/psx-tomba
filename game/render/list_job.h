// game/render/list_job.h — the registers a model list emitter takes (a0 = record list, a1 = OT base, a2 = count, a3).
#pragma once

#include "core.h"

#include <cstdint>

namespace tomba::render {

// The registers an emitter takes and the stack it runs on.
struct ListCall {
  std::uint32_t list = 0;  // a0
  std::uint32_t ot = 0;    // the OT it links into
  std::uint32_t count = 0; // a2
  std::uint32_t a3 = 0;
  std::uint32_t sp = 0;
  std::uint32_t a1 = 0; // `ot` for an emitter handed its OT; another argument for one that reads the active OT

  static ListCall fromRegisters(const Core &core);
};

} // namespace tomba::render
