// game/render/list_job.h — one packet-emitter call as a state producer's saved state.
//
// A model list emitter takes (a0 = record list, a1 = OT base, a2 = count, a3) and links packets from the
// pool into the OT through the GTE. Its call is its own drawing object: `EmitterObject` opens the scope the
// packets are bound to, and `ListJobWriter` saves what a render needs to run the same body again, over host
// memory, with the GTE transform at another t: the call, the GTE control registers, the record bytes and the
// few guest words the body reads that change from frame to frame.
#pragma once

#include "core.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tomba2::render {

// The registers an emitter takes and the stack it runs on.
struct ListCall {
  std::uint32_t list = 0;  // a0
  std::uint32_t ot = 0;    // a1
  std::uint32_t count = 0; // a2
  std::uint32_t a3 = 0;
  std::uint32_t sp = 0;

  static ListCall fromRegisters(const Core &core);
};

// The drawing object of one emitter call: (emitter, the object that reached it), or (emitter, list) when the
// guest called the emitter outside any object. A call twice in one object is ambiguous and keeps its packets.
class EmitterObject {
public:
  EmitterObject(psx::present::EmissionScope &scope, std::uint32_t emitter, std::uint32_t list)
      : guard_(scope, emitter, scope.isOpen() ? scope.current().object : list, 0) {}

private:
  psx::present::EmissionScope::Guard guard_;
};

// The saved call, as a flat byte string.
struct ListJobHeader {
  std::uint32_t variant = 0; // which emitter body, in the family's own numbering
  ListCall call;
  std::uint32_t pool = 0;       // packet cursor at entry
  std::uint32_t arenaBytes = 0; // packet bytes the body may write
  std::uint32_t table = 0;      // OT table and bucket of `call.ot`
  std::uint32_t slot = 0;
  std::uint32_t inputs = 0; // ranges that follow the header
  std::uint32_t control[32] = {};
};

// How an input between two calls of one list moves with t.
enum class InputBlend : std::uint32_t {
  Held,    // the later call's bytes
  Wrapped, // an s16 position that repeats every `modulus`, moved the short way round
};

// One turn of the 12-bit angles the models turn by.
inline constexpr std::uint32_t kAngleTurn = 0x1000u;

struct ListJobInput {
  std::uint32_t address = 0;
  InputBlend blend = InputBlend::Held;
  std::uint32_t modulus = 0;
  std::span<const std::byte> bytes;
};

class ListJobWriter {
public:
  // `call` is read at entry; `arenaBytes` is the most packet bytes the call can write.
  ListJobWriter(Core &core, std::uint32_t variant, const ListCall &call, std::uint32_t arenaBytes);

  // Keeps `size` bytes of guest memory at `address`.
  void input(Core &core,
             std::uint32_t address,
             std::uint32_t size,
             InputBlend blend = InputBlend::Held,
             std::uint32_t modulus = 0);
  // Saves the call under the innermost open object. Nothing is saved when the OT is not a named table.
  void save(Core &core) const;

private:
  ListJobHeader header_;
  bool named_ = false;
  std::vector<std::uint32_t> ranges_; // address, size, blend, modulus per input
  std::vector<std::byte> bytes_;
};

// A saved call, read back.
class ListJob {
public:
  explicit ListJob(std::span<const std::byte> state);

  const ListJobHeader &header() const {
    return header_;
  }
  const std::vector<ListJobInput> &inputs() const {
    return inputs_;
  }

private:
  ListJobHeader header_;
  std::vector<ListJobInput> inputs_;
};

} // namespace tomba2::render
