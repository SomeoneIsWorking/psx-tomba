// The native model emitters against the authentic guest bodies they override: the A00 plain GT3/GT4 and
// its A07/A0L copies and A08's (OverlayGt3Gt4), the A00 ground GT3/GT4 and its A02/A07/A0L copies
// (OverlayGroundGt3Gt4), the A01/A05-A08 lit GT3/GT4 (LitModelEmitter), every
// UnlitModelEmitter copy and A01's sway, scroll and cue emitters (SwayModelEmitter).
// Each case runs the guest body and the native body from the same RAM, scratchpad, registers and GTE
// state on random record lists, at 4:3, and requires byte-identical main RAM and scratchpad and the same
// v0 and sp.
#include "authenticated_image.h"
#include "core/overrides/native_override_catalog.h"
#include "game.h"
#include "gte_registers.h"
#include "hw_bind.h"
#include "lightrec_executor.h"
#include "lit_model_emitter.h"
#include "overlay_ground_gt3gt4.h"
#include "overlay_gt3gt4.h"
#include "psx_exe_image.h"
#include "render/guest_ordering_table.h"
#include "state_render_check.h"
#include "stub_runtime.h"
#include "sway_model_emitter.h"
#include "unlit_model_emitter.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <lucent/log.h>
#include <memory>
#include <optional>
#include <random>
#include <string_view>
#include <vector>

namespace {

constexpr std::uint32_t kModeSlot = 0x80108F9Cu;
constexpr std::uint32_t kRecords = 0x80190000u;
constexpr std::uint32_t kOt = 0x801B0000u;
constexpr std::uint32_t kPool = 0x001A0000u;
constexpr std::uint32_t kStack = 0x801FFF00u;
constexpr std::uint32_t kReturn = 0x80010100u;
constexpr std::uint32_t kBuckets = 0x800u;
constexpr std::uint32_t kChainEnd = 0x00FFFFFFu;
constexpr std::uint32_t kLight = 0x1F800160u;
constexpr std::uint32_t kHideWord = 0x1F80009Cu;
constexpr std::uint32_t kA01SwayPhase = 0x80139004u;
constexpr std::uint32_t kA01ScrollU = 0x801388ECu;
constexpr std::uint32_t kA01ScrollV = 0x801388EEu;
constexpr std::uint32_t kMaxRecords = 48u;
constexpr int kCasesPerEmitter = 200;
constexpr auto kBudget = psx::cpu::ExecutionBudget::fromCycles(50000000u);

struct Emitter {
  std::uint32_t entry;
  std::uint32_t recordBytes;
  bool quad;
};

struct Overlay {
  std::string_view name; // MAIN.EXE for the resident emitters
  std::vector<Emitter> emitters;
};

constexpr std::string_view kResident = "MAIN.EXE";

struct State {
  std::vector<std::uint8_t> ram;
  std::array<std::uint8_t, 0x400> scratch{};
  std::array<std::uint32_t, 32> r{};
};

State capture(const Core &core) {
  State state;
  state.ram.assign(core.ram, core.ram + sizeof(core.ram));
  std::memcpy(state.scratch.data(), core.scratch, sizeof(core.scratch));
  std::copy(std::begin(core.r), std::end(core.r), state.r.begin());
  return state;
}

void restore(Core &core, const State &state) {
  std::memcpy(core.ram, state.ram.data(), sizeof(core.ram));
  std::memcpy(core.scratch, state.scratch.data(), sizeof(core.scratch));
  std::copy(state.r.begin(), state.r.end(), std::begin(core.r));
}

// A fixed camera: a tilted rotation, the retail projection (OFX 160, OFY 120, H 350) and a far colour.
// A short `focal` lets on-screen records sit close enough for the near clamp.
void resetGte(std::uint32_t focal) {
  constexpr std::array<std::uint32_t, 32> kControl = {
      0x00000F80u, 0x0000FE00u, 0x01C00F00u, 0x0F80FE40u, 0x00000F00u, 0u,          0u,          0u,
      0u,          0u,          0u,          0u,          0u,          0x00000180u, 0x00000200u, 0x00000280u,
      0u,          0u,          0u,          0u,          0u,          0x00000300u, 0x00000400u, 0x00000500u,
      160u << 16,  120u << 16,  0u,          0xFFFFFF00u, 0x01400000u, 0x155u,      0x100u,      0u};
  for (std::uint32_t reg = 0; reg < 31u; ++reg) {
    gte_write_ctrl(reg, kControl[reg]);
  }
  gte_write_ctrl(26u, focal);
  for (std::uint32_t reg = 0; reg < 12u; ++reg) {
    gte_write_data(reg, 0u);
  }
}

std::uint32_t pack(std::int32_t low, std::int32_t high) {
  return static_cast<std::uint16_t>(low) | (static_cast<std::uint32_t>(static_cast<std::uint16_t>(high)) << 16);
}

// Random records: corners from far off-screen to on-screen, some behind the camera, any winding,
// any flag byte, so every cull, depth mode, scroll and light path is reached. One in four sits close to
// the camera, inside the near clamp's band.
void writeRecords(Core &core, std::mt19937 &rng, const Emitter &emitter, std::uint32_t count) {
  std::uniform_int_distribution<int> coordinate(-700, 700);
  std::uniform_int_distribution<int> depth(-200, 4000);
  std::uniform_int_distribution<std::uint32_t> word;
  std::uniform_int_distribution<int> flag(0, 9);
  std::uniform_int_distribution<int> closeDepth(40, 300);
  std::uniform_int_distribution<int> quarter(0, 3);
  for (std::uint32_t index = 0; index < count; ++index) {
    const std::uint32_t record = kRecords + index * emitter.recordBytes;
    for (std::uint32_t offset = 0; offset < emitter.recordBytes; offset += 4u) {
      core.mem_w32(record + offset, word(rng));
    }
    // The packets are polygons whatever the case, as the game's records are.
    const std::uint32_t code = (emitter.quad ? 0x38u : 0x30u) | ((word(rng) & 3u) << 1);
    core.mem_w32(record, (core.mem_r32(record) & 0x00FFFFFFu) | (code << 24));
    const int pick = flag(rng);
    const std::uint32_t flagByte = pick < 8 ? static_cast<std::uint32_t>(pick) : (word(rng) & 0xFFu);
    core.mem_w32(record + 4u, (core.mem_r32(record + 4u) & 0x00FFFFFFu) | (flagByte << 24));
    const bool close = quarter(rng) == 0;
    const int x = close ? coordinate(rng) / 16 : coordinate(rng);
    const int y = close ? coordinate(rng) / 16 : coordinate(rng);
    const int z = close ? closeDepth(rng) : depth(rng);
    const int shrink = close ? 8 : 1;
    const auto corner = [&](int spread) {
      return pack(x + coordinate(rng) / (spread * shrink), y + coordinate(rng) / (spread * shrink));
    };
    if (emitter.quad) {
      core.mem_w32(record + 0x14u, corner(4));
      core.mem_w32(record + 0x18u, pack(z, z + coordinate(rng) / (8 * shrink)));
      core.mem_w32(record + 0x1Cu, corner(4));
      core.mem_w32(record + 0x20u, corner(4));
      core.mem_w32(record + 0x24u, pack(z + coordinate(rng) / (8 * shrink), z + coordinate(rng) / (8 * shrink)));
      core.mem_w32(record + 0x28u, corner(4));
    } else {
      core.mem_w32(record + 0x10u, corner(4));
      core.mem_w32(record + 0x14u, pack(z, z + coordinate(rng) / (8 * shrink)));
      core.mem_w32(record + 0x18u, corner(4));
      core.mem_w32(record + 0x1Cu, corner(4));
      core.mem_w32(record + 0x20u, (core.mem_r32(record + 0x20u) & 0xFFFF0000u) | static_cast<std::uint16_t>(z));
    }
  }
}

void prepare(Core &core, std::mt19937 &rng, const Overlay &overlay, const Emitter &emitter) {
  std::uniform_int_distribution<std::uint32_t> count(0u, kMaxRecords);
  std::uniform_int_distribution<int> light(-1500, 1500);
  std::uniform_int_distribution<std::uint32_t> intensity(0u, 0x5000u);
  std::uniform_int_distribution<std::uint32_t> word;
  core.mem_w32(tomba2::render::OrderingTable::kBasePointer, kOt);
  for (std::uint32_t bucket = 0; bucket < kBuckets; ++bucket) {
    core.mem_w32(kOt + bucket * 4u, kChainEnd);
  }
  tomba2::render::PacketPool(core).setCursor(kPool);
  core.mem_w16(OverlayGt3Gt4::kA08Scroll, static_cast<std::uint16_t>(word(rng)));
  for (std::uint32_t offset = 0; offset < 0x400u; offset += 4u) {
    core.mem_w32(0x1F800000u + offset, word(rng));
  }
  for (std::uint32_t axis = 0; axis < 3u; ++axis) {
    core.mem_w16(kLight + axis * 2u, static_cast<std::uint16_t>(light(rng)));
  }
  if ((word(rng) & 1u) != 0u) {
    core.mem_w32(kHideWord, 0u);
  }
  if (overlay.name == "A01") {
    core.mem_w16(kA01SwayPhase, static_cast<std::uint16_t>(word(rng)));
    core.mem_w8(kA01ScrollU, static_cast<std::uint8_t>(word(rng)));
    core.mem_w8(kA01ScrollV, static_cast<std::uint8_t>(word(rng)));
  }
  if (overlay.name == "A06") {
    core.mem_w16(tomba2::render::UnlitModelEmitter::kA06Scroll.uScroll, static_cast<std::uint16_t>(word(rng)));
  }
  const std::uint32_t records = count(rng);
  writeRecords(core, rng, emitter, records);
  for (std::uint32_t reg = 1u; reg < 32u; ++reg) {
    core.r[reg] = word(rng);
  }
  core.r[4] = kRecords;
  core.r[5] = kOt;
  core.r[6] = records;
  core.r[7] = (word(rng) & 1u) != 0u ? 0u : intensity(rng);
  core.r[29] = kStack;
  core.r[31] = kReturn;
}

std::optional<std::uint32_t> firstDifference(const std::uint8_t *a, const std::uint8_t *b, std::size_t size) {
  for (std::size_t offset = 0; offset < size; ++offset) {
    if (a[offset] != b[offset]) {
      return static_cast<std::uint32_t>(offset);
    }
  }
  return std::nullopt;
}

std::uint32_t poolCursor(const State &state) {
  std::uint32_t cursor = 0;
  std::memcpy(&cursor, state.ram.data() + (tomba2::render::PacketPool::kCursor & 0x1FFFFFu), sizeof(cursor));
  return cursor;
}

// The emitter's state render at t = 1 draws the packets its native body just linked, from the saved call
// alone, and writes nothing to the guest.
bool renderMatches(Core &core, const Overlay &overlay, const Emitter &emitter, int run, const State &afterEmit) {
  const psx::present::FrameRecord frame = tomba::test::walkOrderingTable(core, kOt);
  const psx::present::FrameState state = core.frameStates.collect(frame);
  core.frameStates.clear();
  const auto guestPrimitives = tomba::test::primitivesOf(frame, emitter.entry);
  const psx::present::StateProducer *render = core.stateProducers.find(emitter.entry);
  const auto saved = state.find({emitter.entry, kRecords});
  if (render == nullptr || (!guestPrimitives.empty() && !saved)) {
    lucent::error("authentic-emitters",
                  "{} 0x{:08X} case {}: {} packets and no {}",
                  overlay.name,
                  emitter.entry,
                  run,
                  guestPrimitives.size(),
                  render == nullptr ? "render" : "saved call");
    return false;
  }
  if (!saved) {
    return true;
  }
  tomba::test::CollectedSink sink;
  render->render(*saved, *saved, 1.0f, sink);
  const State afterRender = capture(core);
  if (!tomba::test::sameFrame(sink.drawn, guestPrimitives) || afterRender.ram != afterEmit.ram ||
      afterRender.scratch != afterEmit.scratch || afterRender.r != afterEmit.r) {
    lucent::error("authentic-emitters",
                  "{} 0x{:08X} case {}: the t = 1 render drew {} primitives against the guest body's {}, guest {}",
                  overlay.name,
                  emitter.entry,
                  run,
                  sink.drawn.size(),
                  guestPrimitives.size(),
                  afterRender.ram != afterEmit.ram || afterRender.scratch != afterEmit.scratch ||
                          afterRender.r != afterEmit.r
                      ? "memory changed"
                      : "memory untouched");
    return false;
  }
  return true;
}

struct Totals {
  int cases = 0;
  std::uint64_t packets = 0;
};

bool compareEmitter(
    Core &core, psx::cpu::ImageIdentity image, const Overlay &overlay, const Emitter &emitter, Totals &totals) {
  const std::uint32_t seed = emitter.entry;
  std::mt19937 rng(seed);
  for (int run = 0; run < kCasesPerEmitter; ++run) {
    prepare(core, rng, overlay, emitter);
    const State before = capture(core);
    const std::uint32_t focal = run % 4 == 3 ? 40u : 350u;

    resetGte(focal);
    const auto guest = psx::cpu::callOriginal(core, {image, emitter.entry}, kBudget);
    const State guestState = capture(core);

    restore(core, before);
    resetGte(focal);
    std::optional<psx::cpu::ExecutionResult> native;
    {
      // A00's pair runs inside its caller's block scope in the game.
      const psx::present::EmissionScope::Guard caller(core.emission, emitter.entry, kRecords, 0u);
      native = psx::cpu::dispatchGuest(core, emitter.entry, kBudget);
    }
    const State nativeState = capture(core);

    if (!guest.returned() || !native->returned()) {
      lucent::error(
          "authentic-emitters", "{} 0x{:08X} case {}: a call did not return", overlay.name, emitter.entry, run);
      return false;
    }
    const auto ram = firstDifference(guestState.ram.data(), nativeState.ram.data(), guestState.ram.size());
    const auto scratch = firstDifference(guestState.scratch.data(), nativeState.scratch.data(), 0x400u);
    if (ram || scratch || guestState.r[2] != nativeState.r[2] || guestState.r[29] != nativeState.r[29]) {
      lucent::error("authentic-emitters",
                    "{} 0x{:08X} case {} (seed {}, {} records): first RAM diff 0x{:08X}, first scratchpad diff "
                    "0x{:08X} (0xFFFFFFFF/0x1F8FFFFF: none), "
                    "v0 0x{:08X}/0x{:08X}, sp 0x{:08X}/0x{:08X}, pool cursor 0x{:08X}/0x{:08X}",
                    overlay.name,
                    emitter.entry,
                    run,
                    seed,
                    before.r[6],
                    0x80000000u + ram.value_or(0x7FFFFFFFu),
                    0x1F800000u + scratch.value_or(0xFFFFFu),
                    guestState.r[2],
                    nativeState.r[2],
                    guestState.r[29],
                    nativeState.r[29],
                    poolCursor(guestState),
                    poolCursor(nativeState));
      return false;
    }
    if (!renderMatches(core, overlay, emitter, run, nativeState)) {
      return false;
    }
    totals.cases++;
    totals.packets += tomba2::render::PacketPool(core).cursor() - kPool;
  }
  return true;
}

} // namespace

int main(int argc, char **argv) {
  const std::array<std::string_view, 19> names = {"A00",
                                                  "A01",
                                                  "A02",
                                                  "A05",
                                                  "A06",
                                                  "A07",
                                                  "A08",
                                                  "A0A",
                                                  "A0B",
                                                  "A0C",
                                                  "A0D",
                                                  "A0E",
                                                  "A0F",
                                                  "A0G",
                                                  "A0H",
                                                  "A0I",
                                                  "A0J",
                                                  "A0L",
                                                  "SOP"};
  if (argc != 3 + 2 * static_cast<int>(names.size())) {
    lucent::error("authentic-emitters", "expected MAIN.EXE path/digest then a path/digest pair per overlay in `names`");
    return 2;
  }
  const auto exe = tomba::test::readAuthenticatedImage(argv[1], argv[2], psx::cpu::kPsxExeMaxBytes);
  if (!exe) {
    return 2;
  }
  std::array<std::vector<std::uint8_t>, names.size()> overlays;
  for (std::size_t index = 0; index < names.size(); ++index) {
    const auto bytes = tomba::test::readAuthenticatedImage(
        argv[3 + 2 * index], argv[4 + 2 * index], 0x200000u - (kModeSlot & 0x1FFFFFFFu));
    if (!bytes) {
      return 2;
    }
    overlays[index] = *bytes;
  }

  tomba::test::StubRuntime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  game->mods.aspect = ASPECT_4_3; // the guest's 320-column cull, whatever the local settings say
  auto loaded = psx::cpu::loadPsxExeImage(core, *exe, "MAIN.EXE");
  if (!loaded) {
    lucent::error("authentic-emitters", "MAIN.EXE did not map through the shipping loader");
    return 1;
  }
  gte_bind(&core);
  tomba2::render::LitModelEmitter::registerStateRenders(core);
  tomba2::render::UnlitModelEmitter::registerStateRenders(core);
  tomba2::render::SwayModelEmitter::registerStateRenders(core);
  OverlayGt3Gt4::registerStateRenders(core);
  OverlayGroundGt3Gt4::registerStateRenders(core);
  core.otTables.name(tomba::test::kOtTable, kOt, kBuckets, sizeof(std::uint32_t), psx::gpu::OtWalk::HighToLow);
  tomba2::render::LitModelEmitter::registerOverrides();
  tomba2::render::UnlitModelEmitter::registerOverrides();
  tomba2::render::SwayModelEmitter::registerOverrides();
  OverlayGt3Gt4::registerOverrides(game.get());
  OverlayGroundGt3Gt4::registerOverrides(game.get());
  const psx::cpu::ImageIdentity resident = *loaded.identity;
  tomba::native::bindResident(core, resident, loaded.image.physicalText);

  const std::array<Overlay, 20> plan = {{
      {kResident, {{0x8007FDB0u, 0x24u, false}, {0x8008007Cu, 0x2Cu, true}}},
      {"A00",
       {{0x801465ECu, 0x24u, false},
        {0x801467BCu, 0x2Cu, true},
        {0x8013FB88u, 0x24u, false},
        {0x8013FE58u, 0x2Cu, true}}},
      {"A02", {{0x801246A4u, 0x24u, false}, {0x8012496Cu, 0x2Cu, true}}},
      {"A01",
       {{0x801316A8u, 0x24u, false},
        {0x80131BB0u, 0x2Cu, true},
        {0x80130838u, 0x24u, false},
        {0x80130D9Cu, 0x2Cu, true},
        {0x8012F8D8u, 0x24u, false},
        {0x8013000Cu, 0x2Cu, true},
        {0x80132690u, 0x24u, false},
        {0x801329C4u, 0x2Cu, true}}},
      {"A05", {{0x8013544Cu, 0x24u, false}, {0x8013590Cu, 0x2Cu, true}}},
      {"A06",
       {{0x8013BA44u, 0x24u, false},
        {0x8013BD40u, 0x2Cu, true},
        {0x8013C0D8u, 0x24u, false},
        {0x8013C5B4u, 0x2Cu, true},
        {0x8013CF00u, 0x24u, false},
        {0x8013D1E4u, 0x2Cu, true}}},
      {"A07",
       {{0x8012CDF4u, 0x24u, false},
        {0x8012D2B4u, 0x2Cu, true},
        {0x801311D0u, 0x24u, false},
        {0x801313A0u, 0x2Cu, true},
        {0x8012C7E0u, 0x24u, false},
        {0x8012CAA8u, 0x2Cu, true}}},
      {"A08",
       {{0x80129BACu, 0x24u, false},
        {0x8012A06Cu, 0x2Cu, true},
        {0x80140FBCu, 0x24u, false},
        {0x801411D8u, 0x2Cu, true}}},
      {"A0A", {{0x801103F4u, 0x24u, false}, {0x80110698u, 0x2Cu, true}}},
      {"A0B",
       {{0x80112A24u, 0x24u, false},
        {0x80112CF0u, 0x2Cu, true},
        {0x80113150u, 0x24u, false},
        {0x801133F4u, 0x2Cu, true}}},
      {"A0C",
       {{0x80113788u, 0x24u, false},
        {0x80113A54u, 0x2Cu, true},
        {0x80113EB4u, 0x24u, false},
        {0x80114158u, 0x2Cu, true}}},
      {"A0D", {{0x80113748u, 0x24u, false}, {0x801139ECu, 0x2Cu, true}}},
      {"A0E", {{0x80114458u, 0x24u, false}, {0x801146FCu, 0x2Cu, true}}},
      {"A0F", {{0x801157CCu, 0x24u, false}, {0x80115A70u, 0x2Cu, true}}},
      {"A0G", {{0x8010BC40u, 0x24u, false}, {0x8010BF28u, 0x2Cu, true}}},
      {"A0H",
       {{0x8010AF58u, 0x24u, false},
        {0x8010B240u, 0x2Cu, true},
        {0x8010B6BCu, 0x24u, false},
        {0x8010B960u, 0x2Cu, true}}},
      {"A0I",
       {{0x8010B3DCu, 0x24u, false},
        {0x8010B6C4u, 0x2Cu, true},
        {0x8010BB40u, 0x24u, false},
        {0x8010BDE4u, 0x2Cu, true}}},
      {"A0J",
       {{0x8010A3ACu, 0x24u, false},
        {0x8010A69Cu, 0x2Cu, true},
        {0x8010AB20u, 0x24u, false},
        {0x8010ADC4u, 0x2Cu, true}}},
      {"A0L",
       {{0x80112DECu, 0x24u, false},
        {0x80112FBCu, 0x2Cu, true},
        {0x8010AA4Cu, 0x24u, false},
        {0x8010AD40u, 0x2Cu, true}}},
      {"SOP", {{0x801099B4u, 0x24u, false}, {0x80109C80u, 0x2Cu, true}}},
  }};
  std::optional<psx::cpu::ImageIdentity> mode;
  Totals totals;
  int emitters = 0;
  for (const Overlay &overlay : plan) {
    psx::cpu::ImageIdentity image = resident;
    if (overlay.name != kResident) {
      const std::size_t index =
          static_cast<std::size_t>(std::find(names.begin(), names.end(), overlay.name) - names.begin());
      const std::vector<std::uint8_t> &bytes = overlays[index];
      for (std::size_t offset = 0; offset < bytes.size(); ++offset) {
        core.mem_w8(kModeSlot + static_cast<std::uint32_t>(offset), bytes[offset]);
      }
      const std::uint32_t text = kModeSlot & 0x1FFFFFFFu;
      image = tomba::native::activateOverlay(
          core, mode, overlay.name, {text, text + static_cast<std::uint32_t>(bytes.size())});
    }
    for (const Emitter &emitter : overlay.emitters) {
      if (!core.nativeDispatcher().isInstalled({image, emitter.entry})) {
        lucent::error("authentic-emitters", "{} 0x{:08X} has no native owner installed", overlay.name, emitter.entry);
        return 1;
      }
      if (!compareEmitter(core, image, overlay, emitter, totals)) {
        return 1;
      }
      ++emitters;
    }
  }
  const auto &counters = core.lightrecExecutor().counters();
  lucent::info("authentic-emitters",
               "PASS: {} cases over {} emitters byte-identical (RAM, scratchpad, v0, sp); {} packet bytes; "
               "executed_blocks={} fallback_blocks={}",
               totals.cases,
               emitters,
               totals.packets,
               counters.executedBlocks,
               counters.fallback.calls);
  return counters.fallback.calls == 0 ? 0 : 1;
}
