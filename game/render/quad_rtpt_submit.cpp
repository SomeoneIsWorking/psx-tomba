// game/render/quad_rtpt_submit.cpp — QuadRtptSubmit. See quad_rtpt_submit.h.
#include "quad_rtpt_submit.h"

#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "entry/session.h"
#include "gte_registers.h"
#include "guest_ordering_table.h"
#include "horizontal_visibility_cull.h"
#include "list_job.h"

#include <array>
#include <cstdint>

namespace tomba {

// idx 0 copies whole words and skips the shared tail of the other three.
void QuadRtptSubmit::rotateQuadCorners(Core *c) {
  const uint32_t dst = c->r[4];         // a0
  const uint32_t src = c->r[5];         // a1
  const int32_t idx = (int32_t)c->r[6]; // a2: corner/orientation selector

  if (idx == 1) {
    c->mem_w16(dst + 0x0C, c->mem_r16(src + 4));
    c->mem_w16(dst + 0x14, c->mem_r16(src + 0));
    c->mem_w16(dst + 0x1C, c->mem_r16(src + 12));
    c->mem_w16(dst + 0x24, c->mem_r16(src + 8));
    c->mem_w8(dst + 0x0C, (uint8_t)(c->mem_r8(dst + 0x0C) - 1)); // low byte of each corner -1
    c->mem_w8(dst + 0x14, (uint8_t)(c->mem_r8(dst + 0x14) - 1));
    c->mem_w8(dst + 0x1C, (uint8_t)(c->mem_r8(dst + 0x1C) - 1));
    c->mem_w8(dst + 0x24, (uint8_t)(c->mem_r8(dst + 0x24) - 1));
  } else if (idx < 2) {
    if (idx != 0) {
      return;
    }
    c->mem_w32(dst + 0x0C, c->mem_r32(src + 0));
    c->mem_w32(dst + 0x14, c->mem_r32(src + 4));
    c->mem_w16(dst + 0x1C, c->mem_r16(src + 8));
    c->mem_w16(dst + 0x24, c->mem_r16(src + 12));
    return; // idx==0 skips the shared tail below — faithful to guest 0x8003B054
  } else if (idx == 2) {
    c->mem_w16(dst + 0x0C, c->mem_r16(src + 8));
    c->mem_w16(dst + 0x14, c->mem_r16(src + 12));
    c->mem_w16(dst + 0x1C, c->mem_r16(src + 0));
    c->mem_w16(dst + 0x24, c->mem_r16(src + 4));
    c->mem_w8(dst + 0x0D, (uint8_t)(c->mem_r8(dst + 0x0D) - 1)); // high byte of each corner -1
    c->mem_w8(dst + 0x15, (uint8_t)(c->mem_r8(dst + 0x15) - 1));
    c->mem_w8(dst + 0x1D, (uint8_t)(c->mem_r8(dst + 0x1D) - 1));
    c->mem_w8(dst + 0x25, (uint8_t)(c->mem_r8(dst + 0x25) - 1));
  } else {
    if (idx != 3) {
      return;
    }
    c->mem_w16(dst + 0x0C, c->mem_r16(src + 12));
    c->mem_w16(dst + 0x14, c->mem_r16(src + 8));
    c->mem_w16(dst + 0x1C, c->mem_r16(src + 4));
    c->mem_w16(dst + 0x24, c->mem_r16(src + 0));
    // both bytes of each of the 4 corners -1 (two independent byte-decrements each, NOT a u16 -=1 —
    // matches guest 0x8003B054's per-byte store order exactly, borrow behaviour included).
    c->mem_w8(dst + 0x0C, (uint8_t)(c->mem_r8(dst + 0x0C) - 1));
    c->mem_w8(dst + 0x0D, (uint8_t)(c->mem_r8(dst + 0x0D) - 1));
    c->mem_w8(dst + 0x14, (uint8_t)(c->mem_r8(dst + 0x14) - 1));
    c->mem_w8(dst + 0x15, (uint8_t)(c->mem_r8(dst + 0x15) - 1));
    c->mem_w8(dst + 0x1C, (uint8_t)(c->mem_r8(dst + 0x1C) - 1));
    c->mem_w8(dst + 0x1D, (uint8_t)(c->mem_r8(dst + 0x1D) - 1));
    c->mem_w8(dst + 0x24, (uint8_t)(c->mem_r8(dst + 0x24) - 1));
    c->mem_w8(dst + 0x25, (uint8_t)(c->mem_r8(dst + 0x25) - 1));
  }

  // shared tail (idx 1/2/3 only): second word's two halves, UNROTATED (always src+2/src+6).
  c->mem_w16(dst + 0x16, c->mem_r16(src + 6));
  c->mem_w16(dst + 0x0E, c->mem_r16(src + 2));
}

namespace {

using render::ListCall;
using render::OrderingTable;

constexpr std::uint32_t kTemplateWords = 10u;
constexpr std::uint32_t kTemplateBytes = kTemplateWords * 4u;
// Template offsets of each corner's SXY.
constexpr std::array<std::uint32_t, 4> kScreen{8u, 0x10u, 0x18u, 0x20u};
// The 16-byte frame: the GTE flag, AVSZ4 and the bucket as it is worked.
constexpr std::uint32_t kFrameBytes = 16u;
constexpr std::uint32_t kFlagLocal = 0u;
constexpr std::uint32_t kDepthLocal = 4u;
constexpr std::uint32_t kBucketLocal = 8u;

ListCall submitCall(Core &core) {
  return ListCall{.list = core.r[4],
                  .ot = core.mem_r32(OrderingTable::kBasePointer),
                  .count = core.r[6],
                  .a3 = core.r[7],
                  .sp = core.r[29],
                  .a1 = core.r[5]};
}

void submitForGuest(Core *core) {
  const ListCall call = submitCall(*core);
  QuadRtptSubmit::submit(*core, call, horizontal_cull::forDrawWindow(core));
}

} // namespace

void QuadRtptSubmit::submit(const psx::present::EmitMemory &memory,
                            const ListCall &call,
                            const horizontal_cull::Visibility &visible) {
  psx::cpu::Gte &gte = memory.core().gte();
  const std::uint32_t frame = call.sp - kFrameBytes;
  std::array<psx::gte::ProjectedVertex, 4> corners{};
  for (std::uint32_t reg = 0; reg < 6u; ++reg) {
    gte.writeData(psx::gte::kVxy0 + reg, memory.mem_r32(call.a1 + reg * 4u));
  }
  gte.execute(psx::gte::kRtpt);
  memory.mem_w32(frame + kFlagLocal, gte.readControl(psx::gte::kFlag));
  if (static_cast<std::int32_t>(memory.mem_r32(frame + kFlagLocal)) < 0) {
    memory.mem_w32(frame + kBucketLocal, static_cast<std::uint32_t>(OrderingTable::kNoBucket));
    return;
  }
  for (std::uint32_t corner = 0; corner < 3u; ++corner) {
    corners[corner] = gte.projected(corner);
    memory.storeVertex(call.list + kScreen[corner], corners[corner]);
  }
  // The guest stores NCLIP's MAC0 and overwrites it unread: the quad has no back face.
  gte.execute(psx::gte::kNclip);
  memory.mem_w32(frame + kFlagLocal, gte.readData(psx::gte::kMac0));
  gte.writeData(psx::gte::kVxy0, memory.mem_r32(call.a1 + 0x18u));
  gte.writeData(psx::gte::kVz0, memory.mem_r32(call.a1 + 0x1Cu));
  gte.execute(psx::gte::kRtps);
  memory.mem_w32(frame + kFlagLocal, gte.readControl(psx::gte::kFlag));
  if (static_cast<std::int32_t>(memory.mem_r32(frame + kFlagLocal)) < 0) {
    memory.mem_w32(frame + kBucketLocal, static_cast<std::uint32_t>(OrderingTable::kNoBucket));
    return;
  }
  corners[3] = gte.projected(2);
  memory.storeVertex(call.list + kScreen[3], corners[3]);
  gte.execute(psx::gte::kAvsz4);
  const auto depth = static_cast<std::int32_t>(gte.readData(psx::gte::kOtz));
  memory.mem_w32(frame + kDepthLocal, static_cast<std::uint32_t>(depth));
  memory.mem_w32(frame + kBucketLocal, static_cast<std::uint32_t>(depth));
  if (depth < 0) {
    return;
  }
  const std::int32_t biased = depth + static_cast<std::int32_t>(call.count);
  memory.mem_w32(frame + kBucketLocal, static_cast<std::uint32_t>(biased));
  if (biased < 0) {
    return;
  }
  const std::int32_t bucket = OrderingTable::compressDepth(biased);
  memory.mem_w32(frame + kBucketLocal, static_cast<std::uint32_t>(bucket));
  if (!OrderingTable::inDepthRange(bucket)) {
    return;
  }
  std::array<std::uint32_t, 4> xs{};
  std::array<std::uint32_t, 4> ys{};
  for (std::size_t corner = 0; corner < kScreen.size(); ++corner) {
    xs[corner] = memory.mem_r16(call.list + kScreen[corner]);
    ys[corner] = memory.mem_r16(call.list + kScreen[corner] + 2u);
  }
  if (!visible.keepsX(xs.data(), 4, horizontal_cull::Domain::Packed16) ||
      !visible.keepsY(ys.data(), 4, horizontal_cull::Domain::Packed16)) {
    return;
  }
  const render::PacketPool pool(memory);
  const std::uint32_t packet = pool.cursor();
  OrderingTable(memory, memory.mem_r32(OrderingTable::kBasePointer))
      .link({.packet = packet, .words = kTemplateWords - 1u, .bucket = static_cast<std::uint32_t>(bucket)});
  for (std::uint32_t word = 1; word < kTemplateWords; ++word) {
    memory.mem_w32(packet + word * 4u, memory.mem_r32(call.list + word * 4u));
  }
  for (std::size_t corner = 0; corner < kScreen.size(); ++corner) {
    memory.storeVertex(packet + kScreen[corner], corners[corner]);
  }
  pool.setCursor(packet + kTemplateBytes);
}

void QuadRtptSubmit::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declare(0x8003B054u, "QuadRtptSubmit::rotateQuadCorners", &QuadRtptSubmit::rotateQuadCorners);
  natives.declare(kSubmit, "QuadRtptSubmit::submit", &submitForGuest);
}

} // namespace tomba
