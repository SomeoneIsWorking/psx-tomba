#include "sprite_frame.h"

#include "core.h"
#include "game.h"
#include "guest_call.h"
#include "guest_signed.h"
#include "packet_pool.h"
#include "r3000.h"

#include <array>
#include <cstdlib>
#include <lucent/log.h>

namespace tomba1 {
namespace {

using psx::cpu::kSp;
using psx::present::CornerDomain;
using psx::present::ScreenCull;

// The retail keeps a part when its x or x+w is below 0x141 and its y or y+h below 0xF1.
constexpr int kGuestColumns = 0x141;
constexpr int kGuestRows = 0xF1;

constexpr std::uint32_t kSetDrawMode = 0x8005FA60u;
constexpr std::uint32_t kDrawModeBytes = 0x0Cu;
constexpr std::uint32_t kSpriteBytes = 0x14u;
constexpr std::uint32_t kPartStride = 0x10u;
constexpr std::uint32_t kCallerFrame = 0x28u;
// SetSprt's length and code; SetSemiTrans sets bit 1, raw texture bit 0.
constexpr std::uint8_t kSpriteWords = 4u;
constexpr std::uint8_t kSprite = 0x64u;
constexpr std::uint8_t kSemiTransparent = 0x02u;
constexpr std::uint8_t kRawTexture = 0x01u;

// libgpu AddPrim.
void addPrim(Core &core, std::uint32_t slot, std::uint32_t packet) {
  core.mem_w32(packet, (core.mem_r32(packet) & 0xFF000000u) | (core.mem_r32(slot) & 0x00FFFFFFu));
  core.mem_w32(slot, (core.mem_r32(slot) & 0xFF000000u) | (packet & 0x00FFFFFFu));
}

} // namespace

SpriteFrame::Drawn
SpriteFrame::drawParts(Core &c, std::uint32_t entity, const Placement &at, const ScreenCull &cull, Writes writes) {
  const std::uint8_t flags = c.mem_r8(entity + 0x0Du);
  const std::uint32_t table = c.mem_r32(entity + 0x3Cu);
  const std::uint32_t index = c.mem_r16(c.mem_r32(entity + 0x24u) + at.slot * 2u);
  const std::uint32_t entry = table + index * 4u;
  const auto count = static_cast<std::int16_t>(c.mem_r16(entry));
  if (count <= 0) {
    lucent::error("tomba1-sprite-frame",
                  "entity 0x{:08X} frame {} at 0x{:08X} has {} parts; the guest loop would never end",
                  entity,
                  index,
                  entry,
                  count);
    std::abort();
  }
  std::uint32_t part = table + sext16(c.mem_r16(entry + 2u));
  Drawn drawn{.tpage = c.mem_r16(part + 6u)};
  const std::uint8_t code = (flags >> 7u) != 0u ? kSprite | kSemiTransparent : kSprite;
  for (std::int32_t left = count; left != 0; --left, part += kPartStride) {
    const std::uint32_t packet = PacketPool::reserve(c, kSpriteBytes);
    const std::uint32_t width = c.mem_r8(part + 0x0Au);
    const std::uint32_t height = c.mem_r8(part + 0x0Bu);
    const auto x = static_cast<std::uint16_t>(at.x + sext8(c.mem_r8(part + 0x0Eu)));
    const auto y = static_cast<std::uint16_t>(at.y + sext8(c.mem_r8(part + 0x0Fu)) - at.raise);
    const std::array<std::uint32_t, 2> xs{x, (x + width) & 0xFFFFu};
    const std::array<std::uint32_t, 2> ys{y, (y + height) & 0xFFFFu};
    const bool kept = cull.keeps(xs.data(), ys.data(), 2, CornerDomain::Packed16);
    if (kept || writes == Writes::AsGuest) {
      c.mem_w16(packet + 0x10u, static_cast<std::uint16_t>(width));
      c.mem_w16(packet + 0x12u, static_cast<std::uint16_t>(height));
      c.mem_w16(packet + 0x08u, x);
      c.mem_w16(packet + 0x0Au, y);
    }
    if (!kept) {
      continue;
    }
    ++drawn.kept;
    c.mem_w8(packet + 3u, kSpriteWords);
    c.mem_w8(packet + 7u, code | kRawTexture);
    c.mem_w32(packet + 0x0Cu, c.mem_r32(part));
    if ((flags & 1u) != 0u) {
      c.mem_w16(packet + 0x0Eu, c.mem_r16(entity + 8u));
    }
    addPrim(c, at.link, packet);
    c.mem_w32(PacketPool::kCursor, packet + kSpriteBytes);
  }
  return drawn;
}

void SpriteFrame::linkDrawMode(Core &c, const DrawMode &mode) {
  const std::uint32_t packet = PacketPool::reserve(c, kDrawModeBytes);
  const std::uint32_t sp = c.r[kSp];
  c.r[kSp] = sp - kCallerFrame;
  c.mem_w32(c.r[kSp] + 0x10u, 0u);
  psx::cpu::callGuestNow(c, "SpriteFrame::linkDrawMode", kSetDrawMode, packet, 0u, 0u, mode.tpage);
  c.r[kSp] = sp;
  addPrim(c, mode.link, packet);
  c.mem_w32(PacketPool::kCursor, packet + kDrawModeBytes);
}

void SpriteFrame::draw(Core &c, std::uint32_t entity, const Placement &at, const ScreenCull &cull) {
  linkDrawMode(c, {.link = at.link, .tpage = 0u});
  const Drawn drawn = drawParts(c, entity, at, cull);
  linkDrawMode(c, {.link = at.link, .tpage = sext16(c.mem_r16(entity + 0x1Eu)) + sext16(drawn.tpage)});
}

ScreenCull SpriteFrame::cullFor(const Core &core) {
  return {psx::present::recordCanvasColumns(core.game->guestDisplay.plan(), kGuestColumns), kGuestRows};
}

} // namespace tomba1
