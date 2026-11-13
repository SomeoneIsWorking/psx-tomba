#include "render/crop_layers.h"

#include "core.h"
#include "emit_memory.h"
#include "entry/session.h"
#include "ordering_table.h"
#include "render/guest_ordering_table.h"

#include <algorithm>

namespace tomba::render {
namespace {

std::uint32_t poolOffset(Core &core) {
  return psx::gpu::mainRamOffsetOf(PacketPool(psx::present::EmitMemory(core)).cursor());
}

} // namespace

void CropLayers::beginPass() {
  pass_ = {};
}

void CropLayers::beginReplay() {
  replaying_ = true;
  replay_ = {};
}

void CropLayers::endReplays() {
  replaying_ = false;
}

bool CropLayers::uiPacket(std::uint32_t packet) const {
  const std::uint32_t offset = psx::gpu::mainRamOffsetOf(packet);
  const auto holds = [offset](const Named &named) {
    return std::ranges::any_of(named.ui, [offset](const Range &range) {
      return offset >= range.begin && offset < range.end;
    });
  };
  // A replay links the pass's update-block packets as they are, below the pool bytes it takes itself.
  return holds(pass_) || (replaying_ && holds(replay_));
}

void CropLayers::glyph(std::uint32_t packet, std::uint8_t font) {
  auto &glyphs = naming().glyphs;
  const std::uint32_t offset = psx::gpu::mainRamOffsetOf(packet);
  glyphs.emplace(
      std::ranges::upper_bound(glyphs, offset, {}, &std::pair<std::uint32_t, std::uint8_t>::first), offset, font);
}

std::optional<std::uint8_t> CropLayers::glyphFont(std::uint32_t packet) const {
  const std::uint32_t offset = psx::gpu::mainRamOffsetOf(packet);
  const auto fontIn = [offset](const Named &named) -> std::optional<std::uint8_t> {
    const auto glyph =
        std::ranges::lower_bound(named.glyphs, offset, {}, &std::pair<std::uint32_t, std::uint8_t>::first);
    return glyph != named.glyphs.end() && glyph->first == offset ? std::optional(glyph->second) : std::nullopt;
  };
  if (replaying_) {
    if (const std::optional<std::uint8_t> font = fontIn(replay_)) {
      return font;
    }
  }
  return fontIn(pass_);
}

void CropLayers::name(std::uint32_t begin, std::uint32_t end) {
  if (end <= begin) {
    return;
  }
  std::vector<Range> &ranges = naming().ui;
  if (!ranges.empty() && ranges.back().end == begin) {
    ranges.back().end = end;
    return;
  }
  ranges.push_back({.begin = begin, .end = end});
}

CropLayers::Scope::Scope(Core &core) : core_(core), begin_(poolOffset(core)) {
  ++sessionOf(&core_).cropLayers.depth_;
}

CropLayers::Scope::~Scope() {
  CropLayers &layers = sessionOf(&core_).cropLayers;
  if (--layers.depth_ == 0) {
    layers.name(begin_, poolOffset(core_));
  }
}

void uiDrawScoped(Core *core, psx::cpu::NativeFunction body) {
  const CropLayers::Scope scope(*core);
  body(core);
}

} // namespace tomba::render
