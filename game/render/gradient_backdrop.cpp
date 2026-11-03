// game/render/gradient_backdrop.cpp — A0E FUN_80114178. See gradient_backdrop.h.
#include "gradient_backdrop.h"
#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "guest_abi.h"
#include "guest_call.h"
#include "guest_ordering_table.h"
#include "libgpu_draw_mode.h"
#include "wide_window.h"
#include <array>
#include <cstdint>

namespace tomba::render {
namespace {

constexpr GuestFrameSpill kSpills[] = {{17, 0x1C}, {31, 0x20}, {16, 0x18}};
constexpr std::uint32_t kCameraPitch = 0x1F8000F0u; // s16
constexpr std::int32_t kPitchToScreenY = 0x8E8;
constexpr std::uint32_t kBandBytes = 0x24u;
constexpr std::uint32_t kBandCount = 4u;
constexpr std::uint32_t kBandTag = 0x08000000u;
constexpr std::uint32_t kDrawModeTag = 0x02000000u;
constexpr std::uint32_t kDrawModeBytes = 12u;
constexpr std::uint8_t kPolyG4 = 0x38u;
constexpr std::uint32_t kBackgroundBucket = 0x7FFu;
constexpr psx::cpu::GuestCallSite kAfterBands{.entry = 0x80111E14u, .returnPc = 0x8011430Cu};

struct Band {
  std::int32_t top;
  std::int32_t bottom;
  std::uint32_t topColour;
  std::uint32_t bottomColour;
};

} // namespace

void GradientBackdrop::emit(Core *c) {
  GuestFrame<0x28, 3> frame(c, kSpills);
  const psx::present::EmitMemory memory(*c);
  const PacketPool pool(memory);
  const OrderingTable ot = OrderingTable::active(memory);

  const std::int32_t origin = (-(c->mem_r16s(kCameraPitch) * kPitchToScreenY)) >> 12;
  constexpr std::uint32_t kRed = 0xAC0606u;
  constexpr std::uint32_t kPink = 0xEA9898u;
  constexpr std::uint32_t kNavy = 0x390000u;
  const std::array<Band, kBandCount> bands{{
      {origin - 120, origin + 60, kRed, kRed},
      {origin + 60, origin + 120, kRed, kPink},
      {origin + 120, origin + 180, kPink, kNavy},
      {origin + 180, origin + 600, kNavy, kNavy},
  }};
  // Retail spans columns [0, 320); the record canvas widens that to its margins.
  const wide_window::Window window = wide_window::drawWindow(c);

  const std::uint32_t first = pool.cursor();
  std::uint32_t packet = first;
  for (const Band &band : bands) {
    memory.mem_w32(packet + 0x04u, band.topColour | (std::uint32_t{kPolyG4} << 24));
    memory.mem_w16(packet + 0x08u, static_cast<std::uint16_t>(window.left));
    memory.mem_w16(packet + 0x0Au, static_cast<std::uint16_t>(band.top));
    memory.mem_w32(packet + 0x0Cu, band.topColour);
    memory.mem_w16(packet + 0x10u, static_cast<std::uint16_t>(window.right));
    memory.mem_w16(packet + 0x12u, static_cast<std::uint16_t>(band.top));
    memory.mem_w32(packet + 0x14u, band.bottomColour);
    memory.mem_w16(packet + 0x18u, static_cast<std::uint16_t>(window.left));
    memory.mem_w16(packet + 0x1Au, static_cast<std::uint16_t>(band.bottom));
    memory.mem_w32(packet + 0x1Cu, band.bottomColour);
    memory.mem_w16(packet + 0x20u, static_cast<std::uint16_t>(window.right));
    memory.mem_w16(packet + 0x22u, static_cast<std::uint16_t>(band.bottom));
    memory.mem_w32(packet, ot.head(kBackgroundBucket) | kBandTag);
    ot.setHead(kBackgroundBucket, packet);
    packet += kBandBytes;
  }

  const std::uint32_t mode = first + kBandCount * kBandBytes;
  pool.setCursor(mode);
  c->mem_w32(c->r[psx::cpu::kSp] + 0x10u, 0u); // SetDrawMode's fifth argument
  setDrawMode(memory, {.packet = mode, .drawOnDisplay = 0u, .dither = 1u, .texturePage = 0u, .window = 0u});
  memory.mem_w32(mode, ot.head(kBackgroundBucket) | kDrawModeTag);
  ot.setHead(kBackgroundBucket, mode);
  pool.setCursor(mode + kDrawModeBytes);
  psx::cpu::callGuestNow(*c, __func__, kAfterBands, ot.base());
}

void GradientBackdrop::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declareOverlay("A0E", 0x80114178u, "&GradientBackdrop::emit", &GradientBackdrop::emit);
}

} // namespace tomba::render
