// game/render/libgpu_draw_mode.cpp — libgpu SetDrawMode (0x80083DE0). See libgpu_draw_mode.h.
#include "libgpu_draw_mode.h"

namespace tomba2::render {
namespace {

constexpr std::uint32_t kPacketTag = 2u; // words after the tag
constexpr std::uint32_t kDrawModeCommand = 0xE1000000u;
constexpr std::uint32_t kTextureWindowCommand = 0xE2000000u;
constexpr std::uint32_t kTexturePageMask = 0x9FFu;
constexpr std::uint32_t kDitherBit = 0x200u;
constexpr std::uint32_t kDrawOnDisplayBit = 0x400u;

} // namespace

std::uint32_t setDrawMode(const EmitMemory &memory,
                          std::uint32_t packet,
                          std::uint32_t drawOnDisplay,
                          std::uint32_t dither,
                          std::uint32_t texturePage,
                          std::uint32_t window) {
  memory.mem_w8(packet + 3u, kPacketTag);

  std::uint32_t mode = kDrawModeCommand;
  if (dither != 0u) {
    mode |= kDitherBit;
  }
  std::uint32_t page = texturePage & kTexturePageMask;
  if (drawOnDisplay != 0u) {
    page |= kDrawOnDisplayBit;
  }
  mode |= page;
  memory.mem_w32(packet + 4u, mode);

  if (window == 0u) {
    memory.mem_w32(packet + 8u, 0u);
    return mode;
  }

  // The RECT is {u8 maskX @+0, u8 maskY @+2, s16 offX @+4, s16 offY @+6}; offsets are negated.
  std::uint32_t twin = kTextureWindowCommand;
  twin |= (memory.mem_r8(window + 2u) >> 3) << 15;
  twin |= (memory.mem_r8(window + 0u) >> 3) << 10;
  twin |= (static_cast<std::uint32_t>(0 - memory.mem_r16s(window + 6u)) << 2) & 0x3E0u;
  const std::uint32_t offsetX = static_cast<std::uint32_t>(
      static_cast<std::int32_t>(static_cast<std::uint32_t>(0 - memory.mem_r16s(window + 4u)) & 0xFFu) >> 3);
  twin |= offsetX;
  memory.mem_w32(packet + 8u, twin);
  return offsetX;
}

} // namespace tomba2::render
