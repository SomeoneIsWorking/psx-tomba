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
  pass_.clear();
}

void CropLayers::beginReplay() {
  replaying_ = true;
  replay_.clear();
}

void CropLayers::endReplays() {
  replaying_ = false;
}

bool CropLayers::uiPacket(std::uint32_t packet) const {
  const std::uint32_t offset = psx::gpu::mainRamOffsetOf(packet);
  const auto holds = [offset](const std::vector<Range> &ranges) {
    return std::ranges::any_of(ranges, [offset](const Range &range) {
      return offset >= range.begin && offset < range.end;
    });
  };
  // A replay links the pass's update-block packets as they are, below the pool bytes it takes itself.
  return holds(pass_) || (replaying_ && holds(replay_));
}

void CropLayers::name(std::uint32_t begin, std::uint32_t end) {
  if (end <= begin) {
    return;
  }
  std::vector<Range> &ranges = replaying_ ? replay_ : pass_;
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
