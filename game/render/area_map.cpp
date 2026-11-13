// game/render/area_map.cpp — AreaMap, A02 FUN_80113628 and A07 FUN_801140A0. See area_map.h.
#include "area_map.h"

#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "guest_abi.h"
#include "guest_ordering_table.h"
#include "libgpu_draw_mode.h"
#include "list_job.h"
#include "render/crop_layers.h"

namespace tomba::render {
namespace {

constexpr std::uint32_t kPlayerX = 0x1F800160u; // s16
constexpr std::uint32_t kPlayerZ = 0x1F800164u; // s16
constexpr std::uint32_t kClock = 0x1F80017Cu;   // u16
constexpr std::int32_t kWorldPerPixel = 182;
constexpr std::int32_t kMarkerLeft = 0xEC;
constexpr std::uint16_t kBlinkBit = 2;

constexpr std::uint32_t kBucket = 4u;
constexpr std::uint32_t kSpriteBytes = 0x14u;
constexpr std::uint32_t kSpriteWords = 4u;
constexpr std::uint32_t kModeBytes = 0xCu;
constexpr std::uint32_t kModeWords = 2u;

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

// One area's map: which player axis runs across it and which down, and its two sprites.
struct Layout {
  std::uint32_t emitter;
  std::uint32_t across; // s16 player word that moves the marker right
  std::int32_t acrossOrigin;
  std::uint32_t down; // s16 player word that moves the marker, down when `downSign` is 1
  std::int32_t downOrigin;
  std::int32_t markerTop;
  std::int32_t downSign;
  Sprite marker; // x and y are filled in from the player
  Sprite map;
  std::uint32_t texturePage;
};

constexpr Layout kLayouts[] = {
    {.emitter = 0x80113628u, // A02, area type 13
     .across = kPlayerX,
     .acrossOrigin = 0x840,
     .down = kPlayerZ,
     .downOrigin = 0xA7F,
     .markerTop = 0xD0 - 4,
     .downSign = -1,
     .marker = {.code = 0x64, .x = 0, .y = 0, .u = 0x40, .v = 0x68, .clut = 0x3D3F, .size = 8},
     .map = {.code = 0x65, .x = 0xF0, .y = 0x90, .u = 0, .v = 0xB0, .clut = 0x3D7F, .size = 0x40},
     .texturePage = 0xFu},
    {.emitter = 0x801140A0u, // A07, the map turned a quarter: z runs across
     .across = kPlayerZ,
     .acrossOrigin = 0x1680,
     .down = kPlayerX,
     .downOrigin = 0xA00,
     .markerTop = 0xA0,
     .downSign = 1,
     .marker = {.code = 0x64, .x = 0, .y = 0, .u = 0x50, .v = 0x58, .clut = 0x74E7, .size = 8},
     .map = {.code = 0x65, .x = 0xF0, .y = 0xA4, .u = 0xC0, .v = 0x50, .clut = 0x7527, .size = 0x40},
     .texturePage = 0x19u},
};

// The frame spills s0..s3 and ra; SetDrawMode's fifth argument at sp+0x10.
constexpr std::uint32_t kFrameBytes = 0x30u;
constexpr GuestFrameSpill kSpills[5] = {{19, 0x24}, {31 /*ra*/, 0x28}, {18, 0x20}, {17, 0x1C}, {16, 0x18}};
constexpr std::uint32_t kWindowArg = 0x10u;

template <std::uint32_t Variant> void emitForGuest(Core *core) {
  const Layout &layout = kLayouts[Variant];
  const ListCall call{
      .list = 0, .ot = core->mem_r32(OrderingTable::kBasePointer), .count = 0, .a3 = 0, .sp = core->r[29], .a1 = 0};
  ::GuestFrame<kFrameBytes, 5> frame(core, kSpills);
  AreaMap::emit(*core, call, Variant);
  // The down axis's scaling is the body's last mult; it leaves hi/lo guest-visible.
  psx::cpu::multiply(core, core->mem_r16s(layout.down) - layout.downOrigin, static_cast<std::int32_t>(0xB40B40B5u));
}

} // namespace

void AreaMap::emit(const psx::present::EmitMemory &memory, const ListCall &call, std::uint32_t variant) {
  const Layout &layout = kLayouts[variant];
  const OrderingTable ot(memory, call.ot);
  const PacketPool pool(memory);
  const std::int32_t mapAcross = (memory.mem_r16s(layout.across) - layout.acrossOrigin) / kWorldPerPixel;
  const std::int32_t mapDown = (memory.mem_r16s(layout.down) - layout.downOrigin) / kWorldPerPixel;
  const bool blink = (memory.mem_r16(kClock) & kBlinkBit) != 0;

  const std::uint32_t marker = pool.reserve(2u * kSpriteBytes + kModeBytes);
  memory.mem_w8(marker + 4u, blink ? 0x80 : 0xFF);
  memory.mem_w8(marker + 5u, blink ? 0x40 : 0x20);
  memory.mem_w8(marker + 6u, blink ? 0x40 : 0x20);
  Sprite markerSprite = layout.marker;
  markerSprite.x = static_cast<std::int16_t>(mapAcross + kMarkerLeft);
  markerSprite.y = static_cast<std::int16_t>(layout.markerTop + layout.downSign * mapDown);
  drawSprite(memory, ot, marker, markerSprite);
  const std::uint32_t map = marker + kSpriteBytes;
  drawSprite(memory, ot, map, layout.map);
  const std::uint32_t mode = map + kSpriteBytes;
  memory.mem_w32(call.sp - kFrameBytes + kWindowArg, 0);
  setDrawMode(memory,
              {.packet = mode, .drawOnDisplay = 0, .dither = 0, .texturePage = layout.texturePage, .window = 0});
  ot.link({.packet = mode, .words = kModeWords, .bucket = kBucket});
  pool.setCursor(mode + kModeBytes);
}

void AreaMap::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declareOverlay("A02", kLayouts[0].emitter, "AreaMap::emit", render::uiDraw<&emitForGuest<0>>);
  natives.declareOverlay("A07", kLayouts[1].emitter, "AreaMap::emit", render::uiDraw<&emitForGuest<1>>);
}

} // namespace tomba::render
