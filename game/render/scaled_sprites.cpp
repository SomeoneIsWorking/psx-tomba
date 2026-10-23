// game/render/scaled_sprites.cpp — ScaledSprites, extent [0x80027A4C, 0x80027CB4). See scaled_sprites.h.
#include "scaled_sprites.h"

#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "fx_sprite_publish.h"
#include "gte_registers.h"
#include "guest_ordering_table.h"

namespace tomba::render {
namespace {

constexpr std::uint32_t kPacketWords = 9u;
constexpr std::uint32_t kColour = 4u;
constexpr std::uint32_t kScreen[4] = {8u, 0x10u, 0x18u, 0x20u};
constexpr std::uint32_t kUv[4] = {0xCu, 0x14u, 0x1Cu, 0x24u};
constexpr std::uint32_t kClut = 0xEu;
constexpr std::uint32_t kTexturePage = 0x16u;
constexpr std::uint8_t kTexturedQuad = 0x2C;

constexpr std::uint32_t kFlipU = 0x2000u;
constexpr std::uint32_t kFlipV = 0x1000u;
constexpr std::uint32_t kSemi = 0x1u;
constexpr std::uint32_t kPageBits = 0x6u;
constexpr std::uint32_t kClutStep = 0xF00u;
constexpr std::uint32_t kEnd = 0xC000u;
constexpr std::uint32_t kEndOfList = 0x4000u;

// Word 1's high half: flags, grey and steps.
std::uint32_t flagsOf(const psx::present::EmitMemory &memory, std::uint32_t record) {
  return memory.mem_r32(record + 4u) >> 16;
}

// A corner byte's texel edge pair: `near` one cell in, `far` the cell's last texel; flipped swaps them.
struct Edges {
  std::uint8_t near;
  std::uint8_t far;
};
constexpr std::uint32_t kFlippedFarU = 6u;
constexpr std::uint32_t kFlippedFarV = 7u;
Edges edgesOf(std::uint32_t corner, bool flipped, std::uint32_t flippedFar) {
  const std::uint32_t base = corner & 0xF8u;
  const std::uint32_t cells = (corner & 7u) * 8u;
  const auto first = static_cast<std::uint8_t>(base + 1u);
  return flipped ? Edges{static_cast<std::uint8_t>(base + cells + flippedFar), first}
                 : Edges{first, static_cast<std::uint8_t>(base + cells + 7u)};
}

} // namespace

std::uint32_t ScaledSprites::stamp(const psx::present::EmitMemory &memory, const Stamp &stamp) {
  if (!memory.hosted()) {
    Core &core = memory.core();
    core.mem_w32(core.r[29] - 4u, core.r[17]);
    core.mem_w32(core.r[29] - 8u, core.r[16]);
  }
  const FxSpritePublish publish{memory};
  const PacketPool pool(memory);
  psx::cpu::Gte &gte = memory.core().gte();
  gte.writeData(psx::gte::kIr0, memory.mem_r32(fxpublish::kDepthCue));
  const psx::gte::ProjectedVertex anchor = memory.loadVertex(fxpublish::kScreenXY);
  const auto bucket = memory.mem_r32(fxpublish::kOtKey);
  std::uint32_t packet = pool.cursor();
  std::uint32_t record = stamp.records;
  std::uint32_t flags = 0;
  do {
    const std::uint32_t texel = memory.mem_r32(record + 4u);
    flags = texel >> 16;
    const Edges u = edgesOf(texel, (flags & kFlipU) != 0u, kFlippedFarU);
    const Edges v = edgesOf(texel >> 8, (flags & kFlipV) != 0u, kFlippedFarV);
    memory.mem_w8(packet + kUv[0], u.near);
    memory.mem_w8(packet + kUv[2], u.near);
    memory.mem_w8(packet + kUv[1], u.far);
    memory.mem_w8(packet + kUv[3], u.far);
    memory.mem_w8(packet + kUv[0] + 1u, v.near);
    memory.mem_w8(packet + kUv[1] + 1u, v.near);
    memory.mem_w8(packet + kUv[2] + 1u, v.far);
    memory.mem_w8(packet + kUv[3] + 1u, v.far);
    const std::uint32_t grey = flags & 0xFFu;
    const std::uint32_t command = kTexturedQuad | ((flags & kSemi) << 1);
    memory.mem_w32(packet + kColour, (command << 24) | (grey << 16) | (grey << 8) | grey);
    gte.writeData(psx::gte::kRgbc, memory.mem_r32(packet + kColour));
    gte.execute(psx::gte::kDpcs);
    memory.mem_w32(packet + kColour, gte.readData(psx::gte::kRgb2));
    memory.mem_w16(packet + kTexturePage,
                   static_cast<std::uint16_t>((stamp.textureClut >> 16) | ((flags & kPageBits) << 4)));
    memory.mem_w16(packet + kClut, static_cast<std::uint16_t>(stamp.textureClut + ((flags & kClutStep) >> 2)));
    const std::uint32_t offsets = memory.mem_r32(record);
    const auto left = static_cast<std::int8_t>(offsets);
    const auto right = static_cast<std::int8_t>(offsets >> 8);
    const auto top = static_cast<std::int8_t>(offsets >> 16);
    const auto bottom = static_cast<std::int8_t>(offsets >> 24);
    const auto scaleX = static_cast<std::int32_t>(memory.mem_r32(fxpublish::kScaleX));
    const auto scaleY = static_cast<std::int32_t>(memory.mem_r32(fxpublish::kScaleY));
    const std::int8_t xs[4] = {left, right, left, right};
    const std::int8_t ys[4] = {top, top, bottom, bottom};
    for (std::uint32_t corner = 0; corner < 4u; ++corner) {
      memory.storeVertex(packet + kScreen[corner],
                         publish.scaledCorner(
                             {.anchor = anchor, .x = xs[corner], .y = ys[corner], .scaleX = scaleX, .scaleY = scaleY}));
    }
    OrderingTable(memory, stamp.ot).link({.packet = packet, .words = kPacketWords, .bucket = bucket});
    packet += kPacketBytes;
    record += kRecordBytes;
  } while ((flags & kEnd) == 0u);
  pool.setCursor(packet);
  return (flags & kEndOfList) != 0u ? 0u : record;
}

std::uint32_t ScaledSprites::recordCount(const psx::present::EmitMemory &memory, std::uint32_t records) {
  std::uint32_t count = 1;
  while ((flagsOf(memory, records + (count - 1u) * kRecordBytes) & kEnd) == 0u) {
    ++count;
  }
  return count;
}

void ScaledSprites::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declare(kWriter, "ScaledSprites::stamp", [](Core *core) {
    core->r[2] = stamp(
        *core, {.records = core->r[4], .textureClut = core->r[5], .ot = core->mem_r32(OrderingTable::kBasePointer)});
  });
}

} // namespace tomba::render
