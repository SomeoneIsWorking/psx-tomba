#include "overlay_sprite_groups.h"

#include "backdrop_ring.h"
#include "core.h"
#include "game.h"
#include "guest_call.h"
#include "overlay_images.h"
#include "packet_pool.h"
#include "r3000.h"
#include "screen_cull.h"

#include <array>
#include <cstdlib>
#include <lucent/log.h>

namespace tomba1 {
namespace {

using psx::cpu::kA0;
using psx::cpu::kSp;
using psx::cpu::kV0;
using psx::present::CornerDomain;
using psx::present::ScreenCull;

// The retail keeps a part when its x or x+w is below 0x141 and its y or y+h below 0xF1.
constexpr int kGuestColumns = 0x141;
constexpr int kGuestRows = 0xF1;

constexpr std::uint32_t kSetDrawMode = 0x8005FA60u;
// The ordering table pointer; a group links at 0xC90 past it plus four bytes per layer.
constexpr std::uint32_t kOrderingTable = 0x1F8001E0u;
constexpr std::uint32_t kGroupSlots = 0xC90u;

constexpr std::uint32_t kDrawModeBytes = 0x0Cu;
constexpr std::uint32_t kSpriteBytes = 0x14u;
constexpr std::uint32_t kPartStride = 0x10u;
constexpr std::uint32_t kCallerFrame = 0x28u;

std::uint32_t sext8(std::uint8_t value) {
  return static_cast<std::uint32_t>(static_cast<std::int8_t>(value));
}

std::uint32_t sext16(std::uint32_t value) {
  return static_cast<std::uint32_t>(static_cast<std::int16_t>(value));
}

// libgpu AddPrim.
void addPrim(Core &core, std::uint32_t slot, std::uint32_t packet) {
  core.mem_w32(packet, (core.mem_r32(packet) & 0xFF000000u) | (core.mem_r32(slot) & 0x00FFFFFFu));
  core.mem_w32(slot, (core.mem_r32(slot) & 0xFF000000u) | (packet & 0x00FFFFFFu));
}

std::uint32_t layerSlot(Core &core, std::uint32_t entity) {
  return core.mem_r32(kOrderingTable) + sext8(core.mem_r8(entity + 0x0Fu)) * 4u + kGroupSlots;
}

// The frames named by the entity's first two animation slots as raw sprites at `origin`, the second raised
// by e+0x20, then one SetDrawMode for the second frame's first part's tpage.
void drawGroup(Core &c, std::uint32_t entity, BackdropRing::Point origin, const ScreenCull &cull) {
  const std::uint8_t flags = c.mem_r8(entity + 0x0Du);
  std::uint32_t tpage = 0;
  for (std::uint32_t pass = 0; pass < 2u; ++pass) {
    const std::uint32_t table = c.mem_r32(entity + 0x3Cu);
    const std::uint32_t index = c.mem_r16(c.mem_r32(entity + 0x24u) + pass * 2u);
    const std::uint32_t entry = table + index * 4u;
    const auto count = static_cast<std::int16_t>(c.mem_r16(entry));
    if (count <= 0) {
      lucent::error("tomba1-overlay-sprite-groups",
                    "entity 0x{:08X} frame {} at 0x{:08X} has {} parts; the guest loop would never end",
                    entity,
                    index,
                    entry,
                    count);
      std::abort();
    }
    std::uint32_t part = table + sext16(c.mem_r16(entry + 2u));
    tpage = c.mem_r16(part + 6u);
    const std::uint32_t raise = pass == 0u ? 0u : c.mem_r16(entity + 0x20u);
    for (std::int32_t left = count; left != 0; --left, part += kPartStride) {
      const std::uint32_t packet = PacketPool::reserve(c, kSpriteBytes);
      const std::uint32_t width = c.mem_r8(part + 0x0Au);
      const std::uint32_t height = c.mem_r8(part + 0x0Bu);
      const auto x = static_cast<std::uint16_t>(origin.x + sext8(c.mem_r8(part + 0x0Eu)));
      const auto y = static_cast<std::uint16_t>(origin.y + sext8(c.mem_r8(part + 0x0Fu)) - raise);
      c.mem_w16(packet + 0x10u, static_cast<std::uint16_t>(width));
      c.mem_w16(packet + 0x12u, static_cast<std::uint16_t>(height));
      c.mem_w16(packet + 0x08u, x);
      c.mem_w16(packet + 0x0Au, y);
      const std::array<std::uint32_t, 2> xs{x, (x + width) & 0xFFFFu};
      const std::array<std::uint32_t, 2> ys{y, (y + height) & 0xFFFFu};
      if (!cull.keeps(xs.data(), ys.data(), 2, CornerDomain::Packed16)) {
        continue;
      }
      c.mem_w8(packet + 3u, 4u);
      c.mem_w8(packet + 7u, ((flags >> 7u) != 0u ? 0x66u : 0x64u) | 1u);
      c.mem_w32(packet + 0x0Cu, c.mem_r32(part));
      if ((flags & 1u) != 0u) {
        c.mem_w16(packet + 0x0Eu, c.mem_r16(entity + 8u));
      }
      addPrim(c, layerSlot(c, entity), packet);
      c.mem_w32(PacketPool::kCursor, packet + kSpriteBytes);
    }
  }
  const std::uint32_t packet = PacketPool::reserve(c, kDrawModeBytes);
  const std::uint32_t sp = c.r[kSp];
  c.r[kSp] = sp - kCallerFrame;
  c.mem_w32(c.r[kSp] + 0x10u, 0u);
  psx::cpu::callGuestNow(c,
                         "OverlaySpriteGroups::drawGroup",
                         kSetDrawMode,
                         packet,
                         0u,
                         0u,
                         sext16(c.mem_r16(entity + 0x1Eu)) + sext16(tpage));
  c.r[kSp] = sp;
  addPrim(c, layerSlot(c, entity), packet);
  c.mem_w32(PacketPool::kCursor, packet + kDrawModeBytes);
}

// 0x8011DA28 (entity): the group at (e+0x12, e+0x16), then the backdrop ring's positions beyond the picture.
void drawTwoFrameGroup(Core *core) {
  Core &c = *core;
  const std::uint32_t entity = c.r[kA0];
  const ScreenCull cull{psx::present::recordCanvasColumns(c.game->guestDisplay.plan(), kGuestColumns), kGuestRows};
  drawGroup(c, entity, {c.mem_r16(entity + 0x12u), c.mem_r16(entity + 0x16u)}, cull);
  BackdropRing::drawBeyondPicture(
      c, entity, {cull.left(), cull.right()}, [&](std::uint32_t column, BackdropRing::Point at) {
        drawGroup(c, column, at, cull);
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
