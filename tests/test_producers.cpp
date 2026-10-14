// Producers key each drawn primitive by the guest record it belongs to, through the shipping overrides.
#include "core/overrides/native_override_catalog.h"
#include "game.h"
#include "gte_registers.h"
#include "guest_ordering_table.h"
#include "libgpu_draw_mode.h"
#include "lightrec_executor.h"
#include "rain_streaks.h"
#include "sop_ground.h"
#include "state_producer.h"
#include "state_render_check.h"
#include "stub_game_runtime.h"
#include "tile_grid_layer.h"
#include "ui/font.h"

#include <array>
#include <exception>
#include <lucent/log.h>
#include <memory>
#include <optional>
#include <set>
#include <tuple>
#include <vector>

namespace {

// What the product keeps in TombaSession::stateProducers.
psx::present::StateProducerTable stateProducers;

using psx::present::RecordKey;

constexpr GuestAddressRange kResidentText{0x10000u, 0xBE800u};
constexpr GuestAddressRange kModeSlotSop{0x108F9Cu, 0x10D498u};
constexpr GuestAddressRange kModeSlotA08{0x108F9Cu, 0x149F28u};
constexpr std::uint32_t kReturn = 0x80010100u;
constexpr std::uint32_t kPacketPool = tomba::render::PacketPool::kCursor;
constexpr std::uint32_t kOtBaseGlobal = tomba::render::OrderingTable::kBasePointer;
constexpr std::uint32_t kPool = 0x801A0000u;
// The frame's pool cursor is 24-bit (rotatePacketPool), so a linked tag keeps its length byte.
constexpr std::uint32_t kPoolCursor = kPool & 0x00FFFFFFu;
constexpr std::uint32_t kOt = 0x801B0000u;

constexpr std::uint32_t kJrRa = 0x03E00008u;
constexpr std::uint32_t kSwA2A0 = 0xAC860000u;      // sw a2, 0(a0)
constexpr std::uint32_t kV0A0Plus100 = 0x24820100u; // addiu v0, a0, 0x100

int failed = 0;
int checked = 0;

void check(bool condition, const char *name) {
  ++checked;
  if (!condition) {
    ++failed;
    lucent::error("producers-test", "failed: {}", name);
  }
}

void returnStub(Core &core, std::uint32_t entry) {
  core.mem_w32(entry, kJrRa);
  core.mem_w32(entry + 4u, 0u);
}

// A list emitter stand-in: stores its count as the command word of the packet before a0, returns a0 + 0x100.
void listStub(Core &core, std::uint32_t entry) {
  core.mem_w32(entry, kSwA2A0);
  core.mem_w32(entry + 4u, kJrRa);
  core.mem_w32(entry + 8u, kV0A0Plus100);
}

bool call(Core &core, std::uint32_t entry) {
  core.r[29] = 0x801FFF00u;
  core.r[31] = kReturn;
  return psx::cpu::dispatchGuest(core, entry, psx::cpu::ExecutionBudget::fromCycles(1000000u)).returned();
}

void beginFrame(Core &core) {
  core.mem_w32(kPacketPool, kPool);
}

void testSopTileGrid(Core &core) {
  constexpr std::uint32_t kEntry = 0x8010C26Cu;
  constexpr std::uint32_t kNode = 0x80190000u;
  constexpr std::uint32_t kTiles = 0x80191000u;
  constexpr std::uint32_t kWidth = 32u;
  constexpr std::uint32_t kHeight = 16u;
  constexpr std::uint32_t kScript = 0x80192000u;
  core.mem_w8(kNode + 0x10u, kWidth);
  core.mem_w8(kNode + 0x11u, kHeight);
  core.mem_w32(kNode + 0x14u, kTiles);
  for (std::uint32_t cell = 0; cell < kWidth * kHeight; cell++) {
    core.mem_w16(kTiles + cell * 2u, static_cast<std::uint16_t>(cell & 0xFFu));
  }
  core.mem_w8(kScript, 2u);
  core.mem_w8(kScript + 1u, 7u);
  core.mem_w32(0x8010D2FCu, kScript);
  core.mem_w8(0x8010D390u, 1u);
  core.mem_w8(0x8010D391u, 9u);
  core.mem_w8(0x8010D392u, 9u);

  beginFrame(core);
  core.mem_w16(kNode + 0x28u, 160u);
  core.mem_w16(kNode + 0x2Au, 120u);
  core.r[4] = kNode;
  check(call(core, kEntry), "the SOP tile grid returns");
  const std::uint32_t firstX = core.mem_r16(kPool + 8u);
  const std::uint32_t secondX = core.mem_r16(kPool + 16u + 8u);
  check(core.emission.identityFor(kPool) == RecordKey{kEntry, kNode, 0u, 0u} &&
            core.emission.identityFor(kPool + 16u) == RecordKey{kEntry, kNode, 0u, 0u},
        "each tile sprite is keyed by the grid node");
  check(core.mem_r8(0x8010D390u) == 7u && core.mem_r32(0x8010D2FCu) == kScript + 2u && core.mem_r8(0x8010D391u) == 8u,
        "an expired palette cycle reloads its countdown and advances its script; the others count down");

  beginFrame(core);
  core.mem_w16(kNode + 0x28u, 176u);
  core.r[4] = kNode;
  check(call(core, kEntry), "the scrolled SOP tile grid returns");
  check(core.mem_r16(kPool + 8u) == ((secondX - 16u) & 0xFFFFu) && firstX != secondX,
        "after a 16 px scroll the first sprite is the cell that was second, at its moved position");
}

// On the 16:9 record canvas the grid walks the margins too, its map column wrapping modulo the map width.
void testTileGridMargin(Game &game) {
  constexpr std::uint32_t kEntry = 0x8010C26Cu;
  constexpr std::uint32_t kNode = 0x80190000u;
  constexpr int kMargin = 54;
  constexpr std::uint32_t kGuestColumns = 22u; // [0, 320 + 32) in 16 px steps
  constexpr std::uint32_t kCanvasColumns = 29u;
  constexpr std::uint32_t kRows = 16u;
  constexpr std::uint32_t kTileBytes = 16u;
  constexpr std::uint32_t kHeaderBytes = 12u;
  Core &core = game.core;
  // The SOP test tiles are numbered by cell, so a sprite's atlas word names the cell it draws.
  const auto tileUv = [](std::uint32_t cell) {
    return static_cast<std::uint16_t>(((cell & 0xFu) << 4) | ((cell & 0xF0u) << 8));
  };
  for (const int margin : {0, kMargin}) {
    GuestProjectionPlan plan;
    plan.presentationHorizontalMargin = margin;
    game.guestDisplay.latch(plan);
    beginFrame(core);
    core.mem_w16(kNode + 0x28u, 160u);
    core.mem_w16(kNode + 0x2Au, 120u);
    core.r[4] = kNode;
    check(call(core, kEntry), "the SOP tile grid returns");
    const std::uint32_t columns = margin == 0 ? kGuestColumns : kCanvasColumns;
    check(tomba::render::PacketPool(core).cursor() == kPool + columns * kRows * kTileBytes + kHeaderBytes,
          margin == 0 ? "4:3 walks the guest's 22 columns" : "16:9 walks 29 columns across the canvas");
    if (margin == 0) {
      check(core.mem_r16s(kPool + 8u) == -8 && core.mem_r16(kPool + 12u) == 0x0000u,
            "4:3 starts at map column 0, 8 px left of the screen");
    } else {
      check(core.mem_r16s(kPool + 8u) == -72 && core.mem_r16(kPool + 12u) == tileUv(28u),
            "16:9 starts at map column 28 (column -4 of the wrapped map), across the left margin");
    }
  }

  // A map narrower than the canvas shows cells twice, its columns wrapping modulo the map width.
  constexpr std::uint32_t kNarrowWidth = 22u;
  const std::uint8_t mapWidth = core.mem_r8(kNode + 0x10u);
  core.mem_w8(kNode + 0x10u, kNarrowWidth);
  GuestProjectionPlan wide;
  wide.presentationHorizontalMargin = kMargin;
  game.guestDisplay.latch(wide);
  beginFrame(core);
  core.mem_w16(kNode + 0x28u, 160u);
  core.mem_w16(kNode + 0x2Au, 120u);
  core.r[4] = kNode;
  check(call(core, kEntry), "the narrow SOP tile grid returns");
  check(core.mem_r16(kPool + 12u) == tileUv(18u) && core.mem_r16(kPool + 26u * kTileBytes + 12u) == tileUv(0u),
        "column -4 is cell 18 and column 22 is cell 0 again");
  core.mem_w8(kNode + 0x10u, mapWidth);
  game.guestDisplay.latch(GuestProjectionPlan{});
}

// The grid's state render: drawn through the shipping override, saved under its node, rendered over host memory.
constexpr std::uint32_t kGridEntry = 0x8010C26Cu;
constexpr std::uint32_t kGridNode = 0x80190000u;
constexpr std::uint32_t kGridTexturePage = 0x15u;
constexpr std::uint32_t kGridWidth = 32u;
constexpr std::uint32_t kGridHeight = 16u;

psx::present::FrameRecord drawGridFrame(Core &core, std::uint32_t scrollX, std::uint32_t scrollY) {
  core.mem_w32(kPacketPool, kPoolCursor);
  core.mem_w32(kOtBaseGlobal, kOt);
  tomba::test::clearOrderingTable(core, kOt);
  core.mem_w16(kGridNode + 0x04u, kGridTexturePage);
  core.mem_w16(kGridNode + 0x28u, scrollX);
  core.mem_w16(kGridNode + 0x2Au, scrollY);
  core.mem_w16(kGridNode + 0x30u, kGridWidth * 16u);
  core.mem_w16(kGridNode + 0x32u, kGridHeight * 16u);
  core.r[4] = kGridNode;
  check(call(core, kGridEntry), "the SOP tile grid returns");
  return tomba::test::submittedFrame(core, kOt);
}

std::vector<psx::present::DrawPrimitive> composedGrid(const tomba::test::ComposeArgs &args) {
  check(stateProducers.find(kGridEntry) != nullptr && args.to.find({kGridEntry, kGridNode}) &&
            args.from.find({kGridEntry, kGridNode}),
        "the tile grid has a render and a state in both frames");
  return tomba::test::composedPrimitives(stateProducers, args, {kGridEntry, kGridNode});
}

void testTileGridStateRender(Core &core) {
  const psx::present::FrameRecord frame = drawGridFrame(core, 160u, 120u);
  const psx::present::FrameState state = core.frameStates.collect(frame);
  const auto guest = tomba::test::primitivesOf(frame, kGridEntry);
  check(guest.size() == std::size_t{22} * 16u && guest.front().state.texPageX == 5u * 64u &&
            guest.front().state.texPageY == 256u,
        "the SOP grid draws its sprites on the node's texture page");

  check(tomba::test::sameFrame(composedGrid({.shown = frame, .from = state, .to = state}), guest),
        "t = 1 reproduces the sprites the guest wrote, bucket and all");

  // The render reads the saved node only: scramble the node and move the OT the frame was drawn into.
  std::array<std::uint32_t, 15> node{};
  for (std::uint32_t word = 0; word < node.size(); ++word) {
    node[word] = core.mem_r32(kGridNode + word * 4u);
    core.mem_w32(kGridNode + word * 4u, 0xA5A5A5A5u);
  }
  core.mem_w32(kOtBaseGlobal, kOt + 0x1000u);
  check(tomba::test::sameFrame(composedGrid({.shown = frame, .from = state, .to = state}), guest),
        "the render does not depend on the node it was drawn from");
  for (std::uint32_t word = 0; word < node.size(); ++word) {
    core.mem_w32(kGridNode + word * 4u, node[word]);
  }

  const psx::present::FrameState earlier = core.frameStates.collect(drawGridFrame(core, 160u, 120u));
  const psx::present::FrameRecord laterFrame = drawGridFrame(core, 176u, 140u);
  const psx::present::FrameState later = core.frameStates.collect(laterFrame);
  const psx::present::FrameRecord midway = drawGridFrame(core, 168u, 130u);
  const auto between = composedGrid({.shown = laterFrame, .from = earlier, .to = later, .t = 0.5f});
  check(tomba::test::sameFrame(between, tomba::test::primitivesOf(midway, kGridEntry)),
        "t = 0.5 draws the grid with the scroll halfway between the two frames");
  check(!tomba::test::sameFrame(between, tomba::test::primitivesOf(drawGridFrame(core, 160u, 120u), kGridEntry)),
        "the in-between differs from the earlier frame");

  // A scroll that wrapped from the end of the map to its start moves the short way round.
  const psx::present::FrameState beforeWrap = core.frameStates.collect(drawGridFrame(core, 508u, 120u));
  const psx::present::FrameRecord afterWrapFrame = drawGridFrame(core, 4u, 120u);
  const psx::present::FrameState afterWrap = core.frameStates.collect(afterWrapFrame);
  const psx::present::FrameRecord atEdge = drawGridFrame(core, 0u, 120u);
  check(tomba::test::sameFrame(composedGrid({.shown = afterWrapFrame, .from = beforeWrap, .to = afterWrap, .t = 0.5f}),
                               tomba::test::primitivesOf(atEdge, kGridEntry)),
        "a scroll across the map's wrap moves the short way round");
}

void testSopGround(Core &core) {
  constexpr std::uint32_t kEntry = 0x80109FE0u;
  constexpr std::uint32_t kList = 0x80194000u;
  constexpr std::uint32_t kBlocks = 0x80195000u;
  constexpr std::uint32_t kBlockA = kBlocks + 0x10u * 4u;
  constexpr std::uint32_t kBlockB = kBlocks + 0x40u * 4u;
  listStub(core, 0x801099B4u);
  listStub(core, 0x80109C80u);
  core.mem_w32(kList + 0xCu, kBlocks);
  core.mem_w32(kBlockA, 0x00020003u);
  core.mem_w32(kBlockB, 0x00010001u);

  beginFrame(core);
  core.mem_w8(kList + 6u, 2u);
  core.mem_w16(kList + 0x10u, 0x10u);
  core.mem_w16(kList + 0x12u, 0x40u);
  core.r[4] = kList;
  check(call(core, kEntry), "the SOP ground returns");
  check(core.mem_r32(kBlockA + 4u) == 3u && core.mem_r32(kBlockA + 0x104u) == 2u &&
            core.emission.identityFor(kBlockA) == RecordKey{kEntry, kBlockA, 0u, 0u} &&
            core.emission.identityFor(kBlockA + 0x100u) == RecordKey{kEntry, kBlockA, 0u, 0u} &&
            core.emission.identityFor(kBlockB) == RecordKey{kEntry, kBlockB, 0u, 0u},
        "both lists of a visible block are drawn under that block");

  beginFrame(core);
  core.mem_w8(kList + 6u, 1u);
  core.mem_w16(kList + 0x10u, 0x40u);
  core.r[4] = kList;
  check(call(core, kEntry), "the SOP ground returns with one block");
  check(core.emission.identityFor(kBlockB) == RecordKey{kEntry, kBlockB, 0u, 0u},
        "a block keeps its key when it moves to another list slot");
}

constexpr std::uint32_t kGlyphEntry = 0x80078CA8u;
constexpr std::uint32_t kGlyphString = 0x80196000u;
constexpr std::uint32_t kGlyphBucket = 1u;

// "A B" drawn at `x`; the stubbed SetDrawMode wrote nothing, so the string's draw mode packet is filled in here.
psx::present::FrameRecord drawGlyphFrame(Core &core, std::uint32_t x) {
  core.mem_w8(kGlyphString + 0u, 'A');
  core.mem_w8(kGlyphString + 1u, ' ');
  core.mem_w8(kGlyphString + 2u, 'B');
  core.mem_w8(kGlyphString + 3u, 0u);
  core.mem_w32(kPacketPool, kPoolCursor);
  core.mem_w32(kOtBaseGlobal, kOt);
  tomba::test::clearOrderingTable(core, kOt);
  core.r[4] = (40u << 16) | x;
  core.r[5] = 0x00100008u;
  core.r[6] = 8u;
  core.r[7] = kGlyphString;
  core.r[29] = 0x801FFF00u;
  core.mem_w32(core.r[29] + 16u, kGlyphBucket);
  core.r[31] = kReturn;
  check(psx::cpu::dispatchGuest(core, kGlyphEntry, psx::cpu::ExecutionBudget::fromCycles(1000000u)).returned(),
        "the glyph emitter returns");
  constexpr std::uint32_t kGlyphPacketBytes = 0x14u;
  tomba::render::setDrawMode(psx::present::EmitMemory(core),
                             {.packet = kPool + 2u * kGlyphPacketBytes,
                              .drawOnDisplay = 0u,
                              .dither = 0u,
                              .texturePage = tomba::Font::kGlyphTexturePage,
                              .window = 0u});
  return tomba::test::submittedFrame(core, kOt);
}

std::vector<psx::present::DrawPrimitive> composedGlyph(std::uint32_t offset, const tomba::test::ComposeArgs &args) {
  const psx::present::ObjectId glyph{kGlyphEntry, kGlyphString + offset};
  check(stateProducers.find(kGlyphEntry) != nullptr && args.to.find(glyph) && args.from.find(glyph),
        "the glyph has a render and a state in both frames");
  return tomba::test::composedPrimitives(stateProducers, args, glyph);
}

std::vector<psx::present::DrawPrimitive> glyphPrimitives(const psx::present::FrameRecord &record,
                                                         std::uint32_t offset) {
  return tomba::test::objectPrimitives(record, {kGlyphEntry, kGlyphString + offset});
}

void testGlyphs(Core &core) {
  const psx::present::FrameRecord frame = drawGlyphFrame(core, 100u);
  check(core.emission.identityFor(kPool) == RecordKey{kGlyphEntry, kGlyphString, 0u, 0u} &&
            core.emission.identityFor(kPool + 20u) == RecordKey{kGlyphEntry, kGlyphString + 2u, 0u, 0u},
        "each glyph is an object named by its character's address");

  const psx::present::FrameState state = core.frameStates.collect(frame);
  const tomba::test::ComposeArgs exact{.shown = frame, .from = state, .to = state};
  check(tomba::test::sameFrame(composedGlyph(0u, exact), glyphPrimitives(frame, 0u)) &&
            tomba::test::sameFrame(composedGlyph(2u, exact), glyphPrimitives(frame, 2u)) &&
            glyphPrimitives(frame, 2u).size() == 1u,
        "t = 1 reproduces the glyph sprites the guest wrote, bucket and texture page");

  const psx::present::FrameState earlier = core.frameStates.collect(drawGlyphFrame(core, 100u));
  const psx::present::FrameRecord laterFrame = drawGlyphFrame(core, 140u);
  const psx::present::FrameState later = core.frameStates.collect(laterFrame);
  const psx::present::FrameRecord midway = drawGlyphFrame(core, 120u);
  const auto between = composedGlyph(2u, {.shown = laterFrame, .from = earlier, .to = later, .t = 0.5f});
  check(tomba::test::sameFrame(between, glyphPrimitives(midway, 2u)) &&
            !tomba::test::sameFrame(between, glyphPrimitives(frame, 2u)),
        "t = 0.5 draws the glyph halfway between its two positions");
}

// The streaks' state render: each drop is an object, its two screen points the state the render moves.
constexpr std::uint32_t kRainEntry = 0x80116904u;
constexpr std::uint32_t kRainTrails = 0x801485E8u;
constexpr std::uint32_t kRainDrops = 32u;

struct DrawRainFrameArgs {
  std::uint32_t node;
  std::uint32_t shift;
};
psx::present::FrameRecord drawRainFrame(Core &core, const DrawRainFrameArgs &args) {
  auto [node, shift] = args;
  core.mem_w32(kPacketPool, kPoolCursor);
  core.mem_w32(kOtBaseGlobal, kOt);
  tomba::test::clearOrderingTable(core, kOt);
  for (std::uint32_t drop = 0; drop < kRainDrops; ++drop) {
    core.mem_w32(kRainTrails + drop * 4u, (10u << 16) | (10u + shift));
  }
  core.gte().writeControl(psx::gte::kOfx, (160u + shift) << 16);
  core.r[4] = node;
  check(call(core, kRainEntry), "the rain returns");
  // The stubbed SetDrawMode wrote nothing; each streak's DR_TPAGE packet follows its line.
  for (std::uint32_t drop = 0; drop < kRainDrops; ++drop) {
    tomba::render::setDrawMode(psx::present::EmitMemory(core),
                               {.packet = kPool + drop * 0x20u + 0x14u,
                                .drawOnDisplay = 0u,
                                .dither = 1u,
                                .texturePage = 0x15u,
                                .window = 0u});
  }
  return tomba::test::submittedFrame(core, kOt);
}

std::vector<psx::present::DrawPrimitive> dropPrimitives(const psx::present::FrameRecord &record, std::uint32_t drop) {
  return tomba::test::objectPrimitives(record, {kRainEntry, kRainTrails + drop * 4u});
}

std::vector<psx::present::DrawPrimitive> composedDrop(std::uint32_t drop, const tomba::test::ComposeArgs &args) {
  return tomba::test::composedPrimitives(stateProducers, args, {kRainEntry, kRainTrails + drop * 4u});
}

void testRainStateRender(Core &core, std::uint32_t node) {
  const psx::present::FrameRecord frame = drawRainFrame(core, {.node = node, .shift = 0u});
  const psx::present::FrameState state = core.frameStates.collect(frame);
  const tomba::test::ComposeArgs atFrame{.shown = frame, .from = state, .to = state};
  bool exact = true;
  bool scrambledExact = true;
  for (std::uint32_t drop = 0; drop < kRainDrops; ++drop) {
    exact = exact && tomba::test::sameFrame(composedDrop(drop, atFrame), dropPrimitives(frame, drop));
  }
  core.mem_w32(kOtBaseGlobal, kOt + 0x1000u);
  for (std::uint32_t drop = 0; drop < kRainDrops; ++drop) {
    core.mem_w32(kRainTrails + drop * 4u, 0xA5A5A5A5u);
    scrambledExact = scrambledExact && tomba::test::sameFrame(composedDrop(drop, atFrame), dropPrimitives(frame, drop));
  }
  check(exact && tomba::test::primitivesOf(frame, kRainEntry).size() == kRainDrops,
        "t = 1 reproduces every streak the guest wrote, bucket and all");
  check(scrambledExact, "the render does not depend on the trails it was drawn from");

  const psx::present::FrameState earlier = core.frameStates.collect(drawRainFrame(core, {.node = node, .shift = 0u}));
  const psx::present::FrameRecord laterFrame = drawRainFrame(core, {.node = node, .shift = 40u});
  const psx::present::FrameState later = core.frameStates.collect(laterFrame);
  const psx::present::FrameRecord midway = drawRainFrame(core, {.node = node, .shift = 20u});
  const tomba::test::ComposeArgs halfway{.shown = laterFrame, .from = earlier, .to = later, .t = 0.5f};
  bool between = true;
  bool moved = true;
  for (std::uint32_t drop = 0; drop < kRainDrops; ++drop) {
    const auto drawn = composedDrop(drop, halfway);
    between = between && !drawn.empty() && tomba::test::sameFrame(drawn, dropPrimitives(midway, drop));
    moved = moved && !tomba::test::sameFrame(drawn, dropPrimitives(frame, drop));
  }
  check(between, "t = 0.5 draws each streak halfway between its two frames");
  check(moved, "the in-between differs from the earlier frame");
}

void testRain(Core &core) {
  constexpr std::uint32_t kEntry = 0x80116904u;
  constexpr std::uint32_t kNode = 0x80197000u;
  constexpr std::uint32_t kTrails = 0x801485E8u;
  constexpr std::uint32_t kDropBytes = 0x20u; // LINE_G2 + DR_TPAGE
  returnStub(core, 0x80084660u);
  returnStub(core, 0x80084220u);
  returnStub(core, 0x80084690u);
  core.mem_w32(0x801450D8u, 0x7D2B89DDu);
  for (std::uint32_t camera = 0; camera < 3u; camera++) {
    core.mem_w16(0x1F8000D2u + camera * 4u, 0x400u);
  }
  for (std::uint32_t drop = 0; drop < 32u; drop++) {
    core.mem_w32(kTrails + drop * 4u, 0x000A000Au);
  }
  core.gte().bind();
  for (std::uint32_t reg = 0; reg < 5u; reg++) {
    core.gte().writeControl(reg, reg % 2u == 0u ? 0x1000u : 0u); // identity rotation
  }
  core.gte().writeControl(5u, 0u);
  core.gte().writeControl(6u, 0u);
  core.gte().writeControl(7u, 1000u);
  core.gte().writeControl(psx::gte::kH, 100u);

  for (int frame = 0; frame < 2; frame++) {
    beginFrame(core);
    core.r[4] = kNode;
    check(call(core, kEntry), "the rain returns");
    check(core.mem_r32(kPacketPool) == kPool + 32u * kDropBytes, "every drop with a trail draws");
    check(core.emission.identityFor(kPool) == RecordKey{kEntry, kTrails, 0u, 0u} &&
              core.emission.identityFor(kPool + 5u * kDropBytes) == RecordKey{kEntry, kTrails + 5u * 4u, 0u, 0u} &&
              core.emission.identityFor(kPool + 31u * kDropBytes) == RecordKey{kEntry, kTrails + 31u * 4u, 0u, 0u},
          "drop i's streak is the object named by its trail slot in every frame");
  }
  testRainStateRender(core, kNode);
}

} // namespace

namespace {

int runTests() {
  tomba::native::NativeOverrideCatalog natives;
  psx::test::StubGameRuntime runtime;
  auto game = std::make_unique<Game>(runtime, psx::config::Config{});
  Core &core = game->core;
  game->mods.aspect = ASPECT_4_3; // the record path, whatever the local settings say
  core.mem_w32(kOtBaseGlobal, kOt);
  returnStub(core, 0x80083DE0u);
  returnStub(core, 0x80081218u);

  tomba::TileGridLayer::registerOverrides(natives);
  tomba::TileGridLayer::registerStateRenders(stateProducers, core);
  tomba::SopGround::registerOverrides(natives);
  tomba::RainStreaks::registerOverrides(natives);
  tomba::RainStreaks::registerStateRenders(stateProducers);
  tomba::Font::registerOverrides(natives);
  tomba::Font::registerStateRenders(stateProducers);
  const auto resident = core.imageCatalog().activate("resident", kResidentText, 1u);
  natives.bindResident(core, resident, kResidentText);

  std::optional<psx::cpu::ImageIdentity> mode;
  natives.activateOverlay(core, mode, "SOP", kModeSlotSop);
  testSopTileGrid(core);
  testTileGridMargin(*game);
  testTileGridStateRender(core);
  testSopGround(core);
  testGlyphs(core);
  natives.activateOverlay(core, mode, "A08", kModeSlotA08);
  testRain(core);

  lucent::info("producers-test", "checked={} failed={}", checked, failed);
  return failed == 0 ? 0 : 1;
}

} // namespace

int main() {
  try {
    return runTests();
  } catch (const std::exception &error) {
    lucent::error("test", "FAIL: {}", error.what());
    return 1;
  }
}
