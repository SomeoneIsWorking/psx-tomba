// Producers key each drawn primitive by the guest record it belongs to, through the shipping overrides.
#include "core/overrides/native_override_catalog.h"
#include "game.h"
#include "guest_ordering_table.h"
#include "hw_bind.h"
#include "lightrec_executor.h"
#include "rain_streaks.h"
#include "sop_ground.h"
#include "stub_runtime.h"
#include "tile_grid_layer.h"
#include "ui/font.h"

#include <lucent/log.h>
#include <memory>
#include <optional>
#include <set>
#include <tuple>

namespace {

using psx::present::RecordKey;

constexpr GuestAddressRange kResidentText{0x10000u, 0xBE800u};
constexpr GuestAddressRange kModeSlotSop{0x108F9Cu, 0x10D498u};
constexpr GuestAddressRange kModeSlotA08{0x108F9Cu, 0x149F28u};
constexpr std::uint32_t kReturn = 0x80010100u;
constexpr std::uint32_t kPacketPool = tomba2::render::PacketPool::kCursor;
constexpr std::uint32_t kOtBaseGlobal = tomba2::render::OrderingTable::kBasePointer;
constexpr std::uint32_t kPool = 0x801A0000u;
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
  check(core.emission.keyFor(kPool) == RecordKey{kEntry, kTiles, 0u, 0u} &&
            core.emission.keyFor(kPool + 16u) == RecordKey{kEntry, kTiles + 2u, 0u, 0u},
        "each tile sprite is keyed by its map cell");
  check(core.mem_r8(0x8010D390u) == 7u && core.mem_r32(0x8010D2FCu) == kScript + 2u && core.mem_r8(0x8010D391u) == 8u,
        "an expired palette cycle reloads its countdown and advances its script; the others count down");

  beginFrame(core);
  core.mem_w16(kNode + 0x28u, 176u);
  core.r[4] = kNode;
  check(call(core, kEntry), "the scrolled SOP tile grid returns");
  check(core.emission.keyFor(kPool) == RecordKey{kEntry, kTiles + 2u, 0u, 0u} &&
            core.mem_r16(kPool + 8u) == ((secondX - 16u) & 0xFFFFu) && firstX != secondX,
        "after a 16 px scroll the same cell keeps its key at its moved position");
}

// On the 16:9 record canvas the grid walks the margins too, its map column wrapping modulo the map width.
void testTileGridMargin(Game &game) {
  constexpr std::uint32_t kEntry = 0x8010C26Cu;
  constexpr std::uint32_t kNode = 0x80190000u;
  constexpr std::uint32_t kTiles = 0x80191000u;
  constexpr int kMargin = 54;
  constexpr std::uint32_t kGuestColumns = 22u; // [0, 320 + 32) in 16 px steps
  constexpr std::uint32_t kCanvasColumns = 29u;
  constexpr std::uint32_t kRows = 16u;
  constexpr std::uint32_t kTileBytes = 16u;
  constexpr std::uint32_t kHeaderBytes = 12u;
  constexpr std::uint32_t kPreviousLap = 0xFFFFu;
  Core &core = game.core;
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
    check(tomba2::render::PacketPool(core).cursor() == kPool + columns * kRows * kTileBytes + kHeaderBytes,
          margin == 0 ? "4:3 walks the guest's 22 columns" : "16:9 walks 29 columns across the canvas");
    if (margin == 0) {
      check(core.emission.keyFor(kPool) == RecordKey{kEntry, kTiles, 0u, 0u} && core.mem_r16s(kPool + 8u) == -8,
            "4:3 starts at map column 0, 8 px left of the screen");
    } else {
      check(core.emission.keyFor(kPool) == RecordKey{kEntry, kTiles + 28u * 2u, kPreviousLap, 0u} &&
                core.mem_r16s(kPool + 8u) == -72,
            "16:9 starts at map column 28 of the previous lap (column -4), across the left margin");
    }
  }

  // A map narrower than the canvas shows cells twice; the lap keeps each repeat's key distinct.
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
  std::set<std::tuple<std::uint32_t, std::uint32_t>> keys;
  for (std::uint32_t tile = 0; tile < kCanvasColumns * kRows; tile++) {
    const auto key = core.emission.keyFor(kPool + tile * kTileBytes);
    keys.emplace(key ? key->object : 0u, key ? key->element : 0u);
  }
  check(keys.size() == kCanvasColumns * kRows, "every tile of a wrapped map has its own key");
  check(core.emission.keyFor(kPool) == RecordKey{kEntry, kTiles + 18u * 2u, kPreviousLap, 0u} &&
            core.emission.keyFor(kPool + 26u * kTileBytes) == RecordKey{kEntry, kTiles, 1u, 0u},
        "column -4 is cell 18 of the previous lap and column 22 is cell 0 of the next");
  beginFrame(core);
  core.mem_w16(kNode + 0x28u, 176u);
  core.r[4] = kNode;
  check(call(core, kEntry), "the scrolled narrow SOP tile grid returns");
  check(core.emission.keyFor(kPool + 25u * kTileBytes) == RecordKey{kEntry, kTiles, 1u, 0u},
        "a repeated cell keeps its lap as it scrolls");
  core.mem_w8(kNode + 0x10u, mapWidth);
  game.guestDisplay.latch(GuestProjectionPlan{});
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
            core.emission.keyFor(kBlockA) == RecordKey{kEntry, kBlockA, 0u, 0u} &&
            core.emission.keyFor(kBlockA + 0x100u) == RecordKey{kEntry, kBlockA, 0u, 0u} &&
            core.emission.keyFor(kBlockB) == RecordKey{kEntry, kBlockB, 0u, 0u},
        "both lists of a visible block are drawn under that block");

  beginFrame(core);
  core.mem_w8(kList + 6u, 1u);
  core.mem_w16(kList + 0x10u, 0x40u);
  core.r[4] = kList;
  check(call(core, kEntry), "the SOP ground returns with one block");
  check(core.emission.keyFor(kBlockB) == RecordKey{kEntry, kBlockB, 0u, 0u},
        "a block keeps its key when it moves to another list slot");
}

void testGlyphs(Core &core) {
  constexpr std::uint32_t kEntry = 0x80078CA8u;
  constexpr std::uint32_t kString = 0x80196000u;
  core.mem_w8(kString + 0u, 'A');
  core.mem_w8(kString + 1u, ' ');
  core.mem_w8(kString + 2u, 'B');
  core.mem_w8(kString + 3u, 0u);

  beginFrame(core);
  core.r[4] = (40u << 16) | 100u;
  core.r[5] = 0x00100008u;
  core.r[6] = 8u;
  core.r[7] = kString;
  core.r[29] = 0x801FFF00u;
  core.mem_w32(core.r[29] + 16u, 1u);
  core.r[31] = kReturn;
  check(psx::cpu::dispatchGuest(core, kEntry, psx::cpu::ExecutionBudget::fromCycles(1000000u)).returned(),
        "the glyph emitter returns");
  check(core.emission.keyFor(kPool) == RecordKey{kEntry, kString, 0u, 0u} &&
            core.emission.keyFor(kPool + 20u) == RecordKey{kEntry, kString, 2u, 0u},
        "each glyph is keyed by its string and its byte offset in it");
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
  gte_bind(&core);
  for (std::uint32_t reg = 0; reg < 5u; reg++) {
    gte_write_ctrl(reg, 0u);
  }
  gte_write_ctrl(5u, 0u);
  gte_write_ctrl(6u, 0u);
  gte_write_ctrl(7u, 1000u);

  for (int frame = 0; frame < 2; frame++) {
    beginFrame(core);
    core.r[4] = kNode;
    check(call(core, kEntry), "the rain returns");
    check(core.mem_r32(kPacketPool) == kPool + 32u * kDropBytes, "every drop with a trail draws");
    check(core.emission.keyFor(kPool) == RecordKey{kEntry, kNode, 0u, 0u} &&
              core.emission.keyFor(kPool + 5u * kDropBytes) == RecordKey{kEntry, kNode, 5u, 0u} &&
              core.emission.keyFor(kPool + 31u * kDropBytes) == RecordKey{kEntry, kNode, 31u, 0u},
          "drop i's streak is element i of the rain node in every frame");
  }
}

} // namespace

int main() {
  tomba::test::StubRuntime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  game->mods.aspect = ASPECT_4_3; // the record path, whatever the local settings say
  core.mem_w32(kOtBaseGlobal, kOt);
  returnStub(core, 0x80083DE0u);
  returnStub(core, 0x80081218u);

  TileGridLayer::registerOverrides(game.get());
  SopGround::registerOverrides();
  RainStreaks::registerOverrides();
  Font::registerOverrides();
  const auto resident = core.imageCatalog().activate("resident", kResidentText, 1u);
  tomba::native::bindResident(core, resident, kResidentText);

  std::optional<psx::cpu::ImageIdentity> mode;
  tomba::native::activateOverlay(core, mode, "SOP", kModeSlotSop);
  testSopTileGrid(core);
  testTileGridMargin(*game);
  testSopGround(core);
  testGlyphs(core);
  tomba::native::activateOverlay(core, mode, "A08", kModeSlotA08);
  testRain(core);

  lucent::info("producers-test", "checked={} failed={}", checked, failed);
  return failed == 0 ? 0 : 1;
}
