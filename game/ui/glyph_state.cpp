// game/ui/glyph_state.cpp — GlyphState. See glyph_state.h.
#include "glyph_state.h"

#include "gp0_primitive_decode.h"
#include "render/guest_ordering_table.h"
#include "state_producer.h"

#include <cmath>
#include <cstring>
#include <memory>

namespace tomba2::ui {
namespace {

struct Saved {
  std::uint32_t table = 0;
  std::uint32_t slot = 0; // OT slot of bucket 0
  std::uint32_t bucket = 0;
  std::uint32_t command[4] = {}; // colour and code, position, texture coordinate and CLUT, size
};

constexpr unsigned kPositionWord = 1;

std::uint16_t lerpHalf(std::uint32_t from, std::uint32_t to, unsigned shift, float t) {
  const auto a = static_cast<std::int16_t>(from >> shift);
  const auto b = static_cast<std::int16_t>(to >> shift);
  return static_cast<std::uint16_t>(std::lround(a + (b - a) * static_cast<double>(t)));
}

class GlyphRender final : public psx::present::StateProducer {
public:
  void render(std::span<const std::byte> from,
              std::span<const std::byte> to,
              float t,
              psx::present::PrimitiveSink &sink) const override {
    Saved earlier;
    Saved later;
    std::memcpy(&earlier, from.data(), sizeof(earlier));
    std::memcpy(&later, to.data(), sizeof(later));
    std::uint32_t command[4];
    std::memcpy(command, later.command, sizeof(command));
    command[kPositionWord] =
        static_cast<std::uint32_t>(lerpHalf(earlier.command[kPositionWord], later.command[kPositionWord], 0, t)) |
        (static_cast<std::uint32_t>(lerpHalf(earlier.command[kPositionWord], later.command[kPositionWord], 16, t))
         << 16);
    auto glyph = psx::gpu::decodePacketPrimitive(command);
    psx::gpu::applyTexPageAttribute(glyph->state, GlyphState::kTexturePage);
    sink.emit(psx::present::OtSlot{static_cast<std::uint16_t>(later.table), later.slot + later.bucket}, *glyph);
  }
};

} // namespace

void GlyphState::save(Core &core, const std::uint32_t (&command)[4], std::uint32_t bucket) {
  const auto slot = core.otTables.slotOf(render::OrderingTable::active(core).base());
  if (!slot) {
    return;
  }
  Saved saved;
  saved.table = slot->table;
  saved.slot = slot->index;
  saved.bucket = bucket;
  std::memcpy(saved.command, command, sizeof(saved.command));
  core.frameStates.save(core.emission.current(), saved);
}

void GlyphState::registerRender(Core &core, std::uint32_t producer) {
  core.stateProducers.install(producer, std::make_unique<GlyphRender>());
}

} // namespace tomba2::ui
