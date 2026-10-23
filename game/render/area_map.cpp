// game/render/area_map.cpp — AreaMap, A02 FUN_80113628. See area_map.h.
#include "area_map.h"

#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "guest_abi.h"
#include "guest_ordering_table.h"
#include "libgpu_draw_mode.h"
#include "list_state_producer.h"

#include <memory>

namespace tomba::render {
namespace {

constexpr std::uint32_t kPlayerX = 0x1F800160u; // s16
constexpr std::uint32_t kPlayerZ = 0x1F800164u; // s16
constexpr std::uint32_t kClock = 0x1F80017Cu;   // u16
constexpr std::int32_t kOriginX = 0x840;
constexpr std::int32_t kOriginZ = 0xA7F;
constexpr std::int32_t kWorldPerPixel = 182;
constexpr std::int32_t kMarkerLeft = 0xEC;
constexpr std::int32_t kMarkerBottom = 0xD0 - 4;
constexpr std::uint16_t kBlinkBit = 2;

constexpr std::uint32_t kBucket = 4u;
constexpr std::uint32_t kSpriteBytes = 0x14u;
constexpr std::uint32_t kSpriteWords = 4u;
constexpr std::uint32_t kModeBytes = 0xCu;
constexpr std::uint32_t kModeWords = 2u;
constexpr std::uint32_t kTexturePage = 0xFu;
constexpr std::uint32_t kPacketBytes = 2u * kSpriteBytes + kModeBytes;

struct Sprite {
  std::uint8_t code;
  std::int16_t x;
  std::int16_t y;
  std::uint8_t u;
  std::uint8_t v;
  std::uint16_t clut;
  std::uint16_t size;
};

void drawSprite(const psx::present::EmitMemory &memory,
                const OrderingTable &ot,
                std::uint32_t packet,
                const Sprite &sprite) {
  memory.mem_w8(packet + 7u, sprite.code);
  memory.mem_w16(packet + 8u, static_cast<std::uint16_t>(sprite.x));
  memory.mem_w16(packet + 0xAu, static_cast<std::uint16_t>(sprite.y));
  memory.mem_w16(packet + 0xEu, sprite.clut);
  memory.mem_w8(packet + 0xCu, sprite.u);
  memory.mem_w8(packet + 0xDu, sprite.v);
  memory.mem_w16(packet + 0x10u, sprite.size);
  memory.mem_w16(packet + 0x12u, sprite.size);
  ot.link({.packet = packet, .words = kSpriteWords, .bucket = kBucket});
}

// The frame spills s0..s3 and ra; SetDrawMode's fifth argument at sp+0x10.
constexpr std::uint32_t kFrameBytes = 0x30u;
constexpr GuestFrameSpill kSpills[5] = {{19, 0x24}, {31 /*ra*/, 0x28}, {18, 0x20}, {17, 0x1C}, {16, 0x18}};
constexpr std::uint32_t kWindowArg = 0x10u;

void emitForGuest(Core *core) {
  const ListCall call{
      .list = 0, .ot = core->mem_r32(OrderingTable::kBasePointer), .count = 0, .a3 = 0, .sp = core->r[29], .a1 = 0};
  ::GuestFrame<kFrameBytes, 5> frame(core, kSpills);
  const EmitterObject object(*core, AreaMap::kEmitter, OwnObject{AreaMap::kEmitter});
  ListJobWriter job(*core, 0, call, kPacketBytes);
  job.input(*core, kPlayerX, 2, InputBlend::Lanes16);
  job.input(*core, kPlayerZ, 2, InputBlend::Lanes16);
  job.input(*core, kClock, 2);
  AreaMap::emit(*core, call);
  // The z scaling's mult leaves hi/lo guest-visible.
  psx::cpu::multiply(core, core->mem_r16s(kPlayerZ) - kOriginZ, static_cast<std::int32_t>(0xB40B40B5u));
  job.save(*core);
}

class AreaMapProducer final : public ListStateProducer {
public:
  using ListStateProducer::ListStateProducer;

protected:
  void emit(const psx::present::EmitMemory &memory, std::uint32_t, const ListCall &call) const override {
    AreaMap::emit(memory, call);
  }
};

} // namespace

void AreaMap::emit(const psx::present::EmitMemory &memory, const ListCall &call) {
  const OrderingTable ot(memory, call.ot);
  const PacketPool pool(memory);
  const std::int32_t mapX = (memory.mem_r16s(kPlayerX) - kOriginX) / kWorldPerPixel;
  const std::int32_t mapZ = (memory.mem_r16s(kPlayerZ) - kOriginZ) / kWorldPerPixel;
  const bool blink = (memory.mem_r16(kClock) & kBlinkBit) != 0;

  const std::uint32_t marker = pool.cursor();
  memory.mem_w8(marker + 4u, blink ? 0x80 : 0xFF);
  memory.mem_w8(marker + 5u, blink ? 0x40 : 0x20);
  memory.mem_w8(marker + 6u, blink ? 0x40 : 0x20);
  drawSprite(memory,
             ot,
             marker,
             {.code = 0x64,
              .x = static_cast<std::int16_t>(mapX + kMarkerLeft),
              .y = static_cast<std::int16_t>(kMarkerBottom - mapZ),
              .u = 0x40,
              .v = 0x68,
              .clut = 0x3D3F,
              .size = 8});
  const std::uint32_t map = marker + kSpriteBytes;
  drawSprite(memory, ot, map, {.code = 0x65, .x = 0xF0, .y = 0x90, .u = 0, .v = 0xB0, .clut = 0x3D7F, .size = 0x40});
  const std::uint32_t mode = map + kSpriteBytes;
  memory.mem_w32(call.sp - kFrameBytes + kWindowArg, 0);
  setDrawMode(memory, {.packet = mode, .drawOnDisplay = 0, .dither = 0, .texturePage = kTexturePage, .window = 0});
  ot.link({.packet = mode, .words = kModeWords, .bucket = kBucket});
  pool.setCursor(mode + kModeBytes);
}

void AreaMap::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declareOverlay("A02", kEmitter, "AreaMap::emit", &emitForGuest);
}

void AreaMap::registerStateRenders(psx::present::StateProducerTable &renders, Core &core) {
  renders.install(kEmitter, std::make_unique<AreaMapProducer>(core));
}

} // namespace tomba::render
