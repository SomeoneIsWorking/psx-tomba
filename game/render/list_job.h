// game/render/list_job.h — one packet-emitter call as a state producer's saved state.
// A model list emitter takes (a0 = record list, a1 = OT base, a2 = count, a3); ListJobWriter saves what a render needs.
#pragma once

#include "core.h"
#include "gte_control.h"
#include "object_draw.h"
#include "object_effect.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

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

// The saved call, as a flat byte string.
struct ListJobHeader {
  std::uint32_t variant = 0; // which emitter body, in the family's own numbering
  ListCall call;
  std::uint32_t pool = 0;       // packet cursor at entry
  std::uint32_t arenaBytes = 0; // packet bytes the body may write
  std::uint32_t table = 0;      // OT table and bucket of `call.ot`
  std::uint32_t slot = 0;
  std::uint32_t inputs = 0; // ranges that follow the header
  psx::present::GteControl control{};
  ObjectEffect effect; // the dispatching node's, applied over the packets the call wrote
};

// How an input between two calls of one list moves with t.
enum class InputBlend : std::uint8_t {
  Held,    // the later call's bytes
  Wrapped, // an s16 position that repeats every `modulus`, moved the short way round
  Lanes16, // s16 coordinates or sizes, each moved by t
  Camera,  // GTE CR0-7 words, rotation then translation, moved as a transform
};

// One turn of the 12-bit angles the models turn by.
inline constexpr std::uint32_t kAngleTurn = 0x1000u;

struct ListJobInput {
  std::uint32_t address = 0;
  InputBlend blend = InputBlend::Held;
  std::uint32_t modulus = 0;
  std::vector<std::byte> bytes;
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

} // namespace tomba::render
