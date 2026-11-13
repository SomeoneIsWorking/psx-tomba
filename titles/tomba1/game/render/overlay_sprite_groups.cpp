#include "overlay_sprite_groups.h"

#include "backdrop_ring.h"
#include "core.h"
#include "guest_signed.h"
#include "overlay_images.h"
#include "packet_pool.h"
#include "r3000.h"
#include "sprite_frame.h"

namespace tomba1 {
namespace {

using psx::cpu::kA0;
using psx::cpu::kV0;
using psx::present::ScreenCull;

// The ordering table pointer; a group links at 0xC90 past it plus four bytes per layer.
constexpr std::uint32_t kOrderingTable = 0x1F8001E0u;
constexpr std::uint32_t kGroupSlots = 0xC90u;

std::uint32_t layerSlot(Core &core, std::uint32_t entity) {
  return core.mem_r32(kOrderingTable) + sext8(core.mem_r8(entity + 0x0Fu)) * 4u + kGroupSlots;
}

// The frames named by the entity's first two animation slots at `origin`, the second raised by e+0x20, then one
// draw mode for the second frame's first part's tpage. A ring copy the guest never draws emits nothing it culls.
void drawGroup(
    Core &c, std::uint32_t entity, BackdropRing::Point origin, const ScreenCull &cull, SpriteFrame::Writes writes) {
  const std::uint32_t link = layerSlot(c, entity);
  const SpriteFrame::Drawn first =
      SpriteFrame::drawParts(c, entity, {.slot = 0u, .x = origin.x, .y = origin.y, .link = link}, cull, writes);
  const SpriteFrame::Drawn second = SpriteFrame::drawParts(
      c,
      entity,
      {.slot = 1u, .x = origin.x, .y = origin.y, .raise = c.mem_r16(entity + 0x20u), .link = link},
      cull,
      writes);
  if (writes == SpriteFrame::Writes::KeptOnly && first.kept + second.kept == 0u) {
    return;
  }
  SpriteFrame::linkDrawMode(c, {.link = link, .tpage = sext16(c.mem_r16(entity + 0x1Eu)) + sext16(second.tpage)});
}

// 0x8011DA28 (entity): the group at (e+0x12, e+0x16), then the backdrop ring's positions beyond the picture.
void drawTwoFrameGroup(Core *core) {
  Core &c = *core;
  const std::uint32_t entity = c.r[kA0];
  const ScreenCull cull = SpriteFrame::cullFor(c);
  drawGroup(c, entity, {c.mem_r16(entity + 0x12u), c.mem_r16(entity + 0x16u)}, cull, SpriteFrame::Writes::AsGuest);
  BackdropRing::drawBeyondPicture(
      c, entity, {cull.left(), cull.right()}, [&](std::uint32_t column, BackdropRing::Point at) {
        drawGroup(c, column, at, cull, SpriteFrame::Writes::KeptOnly);
      });
  c.r[kV0] = c.mem_r32(PacketPool::kCursor);
}

} // namespace

bool installOverlaySpriteGroups(const InstallOverride &install, std::uint64_t content) {
  if (content != kAreaFourOverlay) {
    return true;
  }
  return install(0x8011DA28u, "OverlaySpriteGroups::drawTwoFrameGroup", drawTwoFrameGroup);
}

} // namespace tomba1
