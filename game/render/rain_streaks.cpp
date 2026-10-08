// game/render/rain_streaks.cpp — FUN_80116904, A08's rain streaks.
#include "rain_streaks.h"

#include "core.h"
#include "core/overrides/guest_jal.h"
#include "core/overrides/native_override_catalog.h"
#include "gte_registers.h"
#include "guest_abi.h"
#include "native_dispatch.h"
#include "ordering_table.h"

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
constexpr std::uint32_t kLattice = 0x7FFu;
constexpr std::uint32_t kLatticeCell = 0x800u;

constexpr std::uint32_t kNodeSeed = 0x50u;
constexpr std::uint32_t kNodeOrigin = 0x2Cu;     // s16 x, y, z
constexpr std::uint32_t kNodeLastOffset = 0x48u; // s16 x, y, z: last frame's lattice offset

constexpr std::uint32_t kCameraPosition = 0x1F8000D2u; // u16 x at +0, y at +4, z at +8
constexpr std::uint32_t kCameraLead = 0x1F800104u;     // s16 x, y, z
constexpr std::uint32_t kCameraMatrix = 0x1F8000F8u;
constexpr std::uint32_t kCameraTranslation = 0x1F80010Cu;
constexpr std::uint32_t kScratchMatrix = 0x1F800000u; // MATRIX; its translation at +0x14
constexpr std::uint32_t kDropVector = 0x1F8000C0u;    // SVECTOR handed to RTPS
constexpr std::uint32_t kScratchDepth = 0x1F800080u;

constexpr std::uint32_t kLineWords = 4u;
constexpr std::uint32_t kLineBytes = 0x14u;
constexpr std::uint32_t kLineColour0 = 0x52FFFFFFu; // LINE_G2, semi-transparent, white
constexpr std::uint32_t kLineColour1 = 0x00808080u;
constexpr std::uint32_t kTpageWords = 2u;
constexpr std::uint32_t kTpageBytes = 0xCu;
constexpr std::uint32_t kTpageMode = 0x15u;

constexpr std::uint32_t kSetRotMatrix = 0x80084660u;
constexpr std::uint32_t kApplyMatrixSv = 0x80084220u;
constexpr std::uint32_t kSetTransMatrix = 0x80084690u;
constexpr std::uint32_t kDrawMode = 0x80083DE0u;

namespace gte = tomba2::gte;
using tomba2::render::OrderingTable;

// FUN_80116904's OTZ: SZ3/4 compressed, kNoBucket outside [4, 0x800).
std::int32_t orderingIndex(std::int32_t sz3) {
  const std::int32_t index = OrderingTable::compressDepth(sz3 >> 2);
  return OrderingTable::inDepthRange(index) ? index : OrderingTable::kNoBucket;
}

} // namespace

void RainStreaks::draw(Core *c) {
  GuestFrame<kFrameBytes, 10> frame(c, kSpills);
  const std::uint32_t sp = c->r[29];
  const std::uint32_t node = c->r[4];
  c->mem_w32(sp + 0x20u, node);
  c->mem_w32(sp + 0x24u, kDropCount);
  c->mem_w32(sp + 0x38u, kNoTrail);

  const std::int32_t leadX = c->mem_r16s(kCameraLead + 0u) >> 1;
  const std::int32_t leadY = c->mem_r16s(kCameraLead + 2u) >> 1;
  const std::int32_t leadZ = c->mem_r16s(kCameraLead + 4u) >> 1;
  std::uint32_t seed = c->mem_r32(node + kNodeSeed);

  // The lattice origin: the camera's 2048-unit cell corner, led by its motion.
  const auto cornerX = static_cast<std::int16_t>(c->mem_r16(kCameraPosition + 0u) - 0x400 + leadX);
  const auto cornerY = static_cast<std::int16_t>(c->mem_r16(kCameraPosition + 4u) - 0x400 + leadY);
  const auto cornerZ = static_cast<std::int16_t>(c->mem_r16(kCameraPosition + 8u) - 0x400 + leadZ);
  c->mem_w16(sp + 0x18u, static_cast<std::uint16_t>(cornerX));
  c->mem_w16(sp + 0x1Au, static_cast<std::uint16_t>(cornerY));
  c->mem_w16(sp + 0x1Cu, static_cast<std::uint16_t>(cornerZ));
  const std::int32_t offsetX = c->mem_r16s(node + kNodeOrigin + 0u) - cornerX;
  const std::int32_t offsetY = c->mem_r16s(node + kNodeOrigin + 2u) - cornerY;
  const std::int32_t offsetZ = c->mem_r16s(node + kNodeOrigin + 4u) - cornerZ;
  const std::int32_t lastX = c->mem_r16s(node + kNodeLastOffset + 0u);
  const std::int32_t lastY = c->mem_r16s(node + kNodeLastOffset + 2u);
  const std::int32_t lastZ = c->mem_r16s(node + kNodeLastOffset + 4u);
  c->mem_w32(sp + 0x28u, static_cast<std::uint32_t>(offsetY));
  c->mem_w32(sp + 0x2Cu, static_cast<std::uint32_t>(lastX));
  c->mem_w32(sp + 0x30u, static_cast<std::uint32_t>(lastY));
  c->mem_w32(sp + 0x34u, static_cast<std::uint32_t>(lastZ));

  c->r[4] = kCameraMatrix;
  tomba::guest::dispatchJalToReturn(*c, kSetRotMatrix, 0x80116A34u);
  c->r[4] = sp + 0x18u;
  c->r[5] = kScratchMatrix + 0x14u;
  tomba::guest::dispatchJalToReturn(*c, kApplyMatrixSv, 0x80116A48u);
  for (std::uint32_t axis = 0; axis < 3u; axis++) {
    const std::uint32_t word = kScratchMatrix + 0x14u + axis * 4u;
    c->mem_w32(word, c->mem_r32(word) + c->mem_r32(kCameraTranslation + axis * 4u));
  }
  c->r[4] = kScratchMatrix;
  tomba::guest::dispatchJalToReturn(*c, kSetTransMatrix, 0x80116A84u);

  const tomba2::render::PacketPool packets(*c);
  std::uint32_t pool = packets.cursor();
  for (std::uint32_t drop = 0; drop < kDropCount; drop++) {
    const auto element = c->emission.element(drop);
    const std::uint32_t trail = kDropTrails + drop * 4u;
    const auto multiplier = static_cast<std::int32_t>(c->mem_r32(kLcgMultiplier));
    const std::int32_t latticeX = static_cast<std::int32_t>(seed) >> 16;
    seed = static_cast<std::uint32_t>(guest_mult(c, static_cast<std::int32_t>(seed), multiplier)) + 1u;
    const std::int32_t latticeY = static_cast<std::int32_t>(seed) >> 16;
    seed = static_cast<std::uint32_t>(guest_mult(c, static_cast<std::int32_t>(seed), multiplier)) + 1u;
    const std::int32_t latticeZ = static_cast<std::int32_t>(seed) >> 16;
    const std::int32_t x = latticeX + offsetX;
    const std::int32_t y = latticeY + offsetY;
    const std::int32_t z = latticeZ + offsetZ;
    c->mem_w16(kDropVector + 0u, static_cast<std::uint16_t>(x & kLattice));
    c->mem_w16(kDropVector + 2u, static_cast<std::uint16_t>(y & kLattice));
    c->mem_w16(kDropVector + 4u, static_cast<std::uint16_t>(z & kLattice));
    gte_write_data(gte::kVxy0, c->mem_r32(kDropVector + 0u));
    gte_write_data(gte::kVz0, c->mem_r32(kDropVector + 4u));
    seed = static_cast<std::uint32_t>(guest_mult(c, static_cast<std::int32_t>(seed), multiplier)) + 1u;
    gte_op(c, gte::kRtps);
    c->mem_w32(kScratchDepth, gte_read_ctrl(gte::kFlag));
    if (static_cast<std::int32_t>(c->mem_r32(kScratchDepth)) < 0) {
      c->mem_w32(trail, kNoTrail);
      continue;
    }
    gte_store_xy(c, pool + 8u, gte::kSxy2);
    const std::int32_t otz = orderingIndex(static_cast<std::int32_t>(gte_read_data(gte::kSz3)));
    c->mem_w32(kScratchDepth, static_cast<std::uint32_t>(otz));
    const bool wrapped = (((x ^ (latticeX + lastX)) | (y ^ (latticeY + lastY)) | (z ^ (latticeZ + lastZ))) &
                          static_cast<std::int32_t>(kLatticeCell)) != 0;
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
    OrderingTable::active(*c).link(pool, kLineWords, static_cast<std::uint32_t>(otz));
    c->mem_w16(trail + 0u, screenX);
    c->mem_w16(trail + 2u, screenY);
    pool += kLineBytes;
    c->r[4] = pool;
    c->r[5] = 0u;
    c->r[6] = 1u;
    c->r[7] = kTpageMode;
    c->mem_w32(sp + 0x10u, 0u);
    tomba::guest::dispatchJalToReturn(*c, kDrawMode, 0x80116C8Cu);
    OrderingTable::active(*c).link(pool, kTpageWords, c->mem_r32(kScratchDepth));
    pool += kTpageBytes;
  }
  c->mem_w32(sp + 0x24u, 0u);
  packets.setCursor(pool);
  c->mem_w16(node + kNodeLastOffset + 0u, static_cast<std::uint16_t>(offsetX));
  c->mem_w16(node + kNodeLastOffset + 2u, static_cast<std::uint16_t>(offsetY));
  c->mem_w16(node + kNodeLastOffset + 4u, static_cast<std::uint16_t>(offsetZ));
}

void RainStreaks::registerOverrides() {
  tomba::native::declareOverlayOverride(
      "A08", kEntry, "RainStreaks::draw", &RainStreaks::draw, psx::present::Producer{psx::present::Arg::A0});
}
