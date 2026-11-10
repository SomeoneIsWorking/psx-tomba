// game/render/rain_streaks.cpp — FUN_80116904, A08's rain streaks.
#include "rain_streaks.h"

#include "camera_lattice.h"
#include "core.h"
#include "core/overrides/guest_jal.h"
#include "core/overrides/native_override_catalog.h"
#include "gte_registers.h"
#include "guest_abi.h"
#include "guest_ordering_table.h"
#include "native_dispatch.h"

namespace tomba {

namespace {

constexpr std::uint32_t kEntry = 0x80116904u;
constexpr GuestFrameSpill kSpills[] = {{31, 0x64},
                                       {30, 0x60},
                                       {23, 0x5C},
                                       {22, 0x58},
                                       {21, 0x54},
                                       {20, 0x50},
                                       {19, 0x4C},
                                       {18, 0x48},
                                       {17, 0x44},
                                       {16, 0x40}};
constexpr std::uint32_t kFrameBytes = 0x68u;

constexpr std::uint32_t kDropCount = 32u;
constexpr std::uint32_t kDropTrails = 0x801485E8u; // per drop {s16 x, s16 y} of its last screen point
constexpr std::uint32_t kNoTrail = 0x7FFF7FFFu;
constexpr std::int16_t kNoTrailX = 0x7FFF;
constexpr std::uint32_t kLcgMultiplier = 0x801450D8u;

constexpr std::uint32_t kNodeSeed = tomba::render::CameraLattice::kNodeSeed;
constexpr std::uint32_t kNodeLastOffset = 0x48u; // s16 x, y, z: last frame's lattice offset

constexpr std::uint32_t kDropVector = 0x1F8000C0u; // SVECTOR handed to RTPS
constexpr std::uint32_t kScratchDepth = 0x1F800080u;

constexpr std::uint32_t kLineWords = 4u;
constexpr std::uint32_t kLineBytes = 0x14u;
constexpr std::uint32_t kLineColour0 = 0x52FFFFFFu; // LINE_G2, semi-transparent, white
constexpr std::uint32_t kLineColour1 = 0x00808080u;
constexpr std::uint32_t kTpageWords = 2u;
constexpr std::uint32_t kTpageBytes = 0xCu;
constexpr std::uint32_t kTpageMode = 0x15u;

constexpr std::uint32_t kDrawMode = 0x80083DE0u;

using tomba::render::CameraLattice;
using tomba::render::OrderingTable;

// FUN_80116904's OTZ: SZ3/4 compressed, kNoBucket outside [4, 0x800).
std::int32_t orderingIndex(std::int32_t sz3) {
  return OrderingTable::bucketOf(sz3 >> 2);
}

} // namespace

void RainStreaks::draw(Core *c) {
  GuestFrame<kFrameBytes, 10> frame(c, kSpills);
  const std::uint32_t sp = c->r[29];
  const std::uint32_t node = c->r[4];
  c->mem_w32(sp + 0x20u, node);
  c->mem_w32(sp + 0x24u, kDropCount);
  c->mem_w32(sp + 0x38u, kNoTrail);

  std::uint32_t seed = c->mem_r32(node + kNodeSeed);

  const psx::present::EmitMemory memory(*c);
  const CameraLattice::Point corner = CameraLattice::corner(memory);
  c->mem_w16(sp + 0x18u, static_cast<std::uint16_t>(corner.x));
  c->mem_w16(sp + 0x1Au, static_cast<std::uint16_t>(corner.y));
  c->mem_w16(sp + 0x1Cu, static_cast<std::uint16_t>(corner.z));
  const CameraLattice::Offset offset = CameraLattice::nodeOffset(memory, node, corner);
  const std::int32_t offsetX = offset.x;
  const std::int32_t offsetY = offset.y;
  const std::int32_t offsetZ = offset.z;
  const std::int32_t lastX = c->mem_r16s(node + kNodeLastOffset + 0u);
  const std::int32_t lastY = c->mem_r16s(node + kNodeLastOffset + 2u);
  const std::int32_t lastZ = c->mem_r16s(node + kNodeLastOffset + 4u);
  c->mem_w32(sp + 0x28u, static_cast<std::uint32_t>(offsetY));
  c->mem_w32(sp + 0x2Cu, static_cast<std::uint32_t>(lastX));
  c->mem_w32(sp + 0x30u, static_cast<std::uint32_t>(lastY));
  c->mem_w32(sp + 0x34u, static_cast<std::uint32_t>(lastZ));

  CameraLattice::loadView(memory, corner);

  const tomba::render::PacketPool packets(*c);
  std::uint32_t pool = packets.cursor();
  for (std::uint32_t drop = 0; drop < kDropCount; drop++) {
    const std::uint32_t trail = kDropTrails + drop * 4u;
    const auto streakScope = c->emission.instance(trail);
    const auto multiplier = static_cast<std::int32_t>(c->mem_r32(kLcgMultiplier));
    const std::int32_t latticeX = static_cast<std::int32_t>(seed) >> 16;
    seed = static_cast<std::uint32_t>(psx::cpu::multiply(c, static_cast<std::int32_t>(seed), multiplier)) + 1u;
    const std::int32_t latticeY = static_cast<std::int32_t>(seed) >> 16;
    seed = static_cast<std::uint32_t>(psx::cpu::multiply(c, static_cast<std::int32_t>(seed), multiplier)) + 1u;
    const std::int32_t latticeZ = static_cast<std::int32_t>(seed) >> 16;
    const std::int32_t x = latticeX + offsetX;
    const std::int32_t y = latticeY + offsetY;
    const std::int32_t z = latticeZ + offsetZ;
    c->mem_w16(kDropVector + 0u, static_cast<std::uint16_t>(x & CameraLattice::kExtent));
    c->mem_w16(kDropVector + 2u, static_cast<std::uint16_t>(y & CameraLattice::kExtent));
    c->mem_w16(kDropVector + 4u, static_cast<std::uint16_t>(z & CameraLattice::kExtent));
    c->gte().writeData(psx::gte::kVxy0, c->mem_r32(kDropVector + 0u));
    c->gte().writeData(psx::gte::kVz0, c->mem_r32(kDropVector + 4u));
    seed = static_cast<std::uint32_t>(psx::cpu::multiply(c, static_cast<std::int32_t>(seed), multiplier)) + 1u;
    c->gte().execute(psx::gte::kRtps);
    c->mem_w32(kScratchDepth, c->gte().readControl(psx::gte::kFlag));
    if (static_cast<std::int32_t>(c->mem_r32(kScratchDepth)) < 0) {
      c->mem_w32(trail, kNoTrail);
      continue;
    }
    c->mem_w32(pool + 8u, c->gte().projected(2).word());
    const std::int32_t otz = orderingIndex(static_cast<std::int32_t>(c->gte().readData(psx::gte::kSz3)));
    c->mem_w32(kScratchDepth, static_cast<std::uint32_t>(otz));
    const bool wrapped = (((x ^ (latticeX + lastX)) | (y ^ (latticeY + lastY)) | (z ^ (latticeZ + lastZ))) &
                          static_cast<std::int32_t>(CameraLattice::kCell)) != 0;
    if (otz < 0 || wrapped) {
      c->mem_w32(trail, kNoTrail);
      continue;
    }
    const std::uint16_t screenX = c->mem_r16(pool + 8u);
    const std::uint16_t screenY = c->mem_r16(pool + 10u);
    if (c->mem_r16s(trail) == kNoTrailX) {
      c->mem_w16(trail + 0u, screenX);
      c->mem_w16(trail + 2u, screenY);
      continue;
    }
    c->mem_w32(pool + 4u, kLineColour0);
    c->mem_w32(pool + 12u, kLineColour1);
    const std::int32_t backX = (c->mem_r16s(trail + 0u) - static_cast<std::int16_t>(screenX)) * 2;
    const std::int32_t backY = (c->mem_r16s(trail + 2u) - static_cast<std::int16_t>(screenY)) * 2;
    c->mem_w16(pool + 16u, static_cast<std::uint16_t>(screenX + backX));
    c->mem_w16(pool + 18u, static_cast<std::uint16_t>(screenY + backY));
    OrderingTable::active(*c).link({.packet = pool, .words = kLineWords, .bucket = static_cast<std::uint32_t>(otz)});
    c->mem_w16(trail + 0u, screenX);
    c->mem_w16(trail + 2u, screenY);
    pool += kLineBytes;
    c->r[4] = pool;
    c->r[5] = 0u;
    c->r[6] = 1u;
    c->r[7] = kTpageMode;
    c->mem_w32(sp + 0x10u, 0u);
    tomba::guest::dispatchJalToReturn(*c, {.target = kDrawMode, .returnAddress = 0x80116C8Cu});
    OrderingTable::active(*c).link({.packet = pool, .words = kTpageWords, .bucket = c->mem_r32(kScratchDepth)});
    pool += kTpageBytes;
  }
  c->mem_w32(sp + 0x24u, 0u);
  packets.setCursor(pool);
  c->mem_w16(node + kNodeLastOffset + 0u, static_cast<std::uint16_t>(offsetX));
  c->mem_w16(node + kNodeLastOffset + 2u, static_cast<std::uint16_t>(offsetY));
  c->mem_w16(node + kNodeLastOffset + 4u, static_cast<std::uint16_t>(offsetZ));
}

void RainStreaks::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declareOverlay(
      "A08", kEntry, "RainStreaks::draw", &RainStreaks::draw, psx::present::Producer{psx::present::Arg::A0});
}

} // namespace tomba
