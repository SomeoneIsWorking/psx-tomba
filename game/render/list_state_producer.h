// game/render/list_state_producer.h — the render of an emitter call saved as a ListJob.
//
// The render runs the emitter's own body again over host memory with the GTE transform between the two saved
// calls, then reads the packets it linked out of the host OT as record primitives in the buckets it linked them
// into. Nothing in the guest is written: the body's packets, OT heads, pool cursor, stack and scratchpad words
// are host bytes, and its records are the saved copy.
#pragma once

#include "emit_memory.h"
#include "list_job.h"
#include "state_producer.h"

#include <cstdint>
#include <span>

namespace tomba2::render {

class ListStateProducer : public psx::present::StateProducer {
public:
  explicit ListStateProducer(Core &core) : core_(core) {}

  void render(std::span<const std::byte> from,
              std::span<const std::byte> to,
              float t,
              psx::present::PrimitiveSink &sink) const final;

protected:
  Core &core() const {
    return core_;
  }
  // Runs emitter body `variant` over `memory`: the packets it links into the OT are the render.
  virtual void emit(const EmitMemory &memory, std::uint32_t variant, const ListCall &call) const = 0;

private:
  Core &core_;
};

} // namespace tomba2::render
