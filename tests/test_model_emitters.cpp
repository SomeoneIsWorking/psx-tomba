// The native model emitters (LitModelEmitter, UnlitModelEmitter, SwayModelEmitter, OverlayGt3Gt4's A08
// pair): their keys, their screen cull across the record canvas, and A08's texture scroll, through the
// shipping overrides.
#include "core/overrides/native_override_catalog.h"
#include "game.h"
#include "gte_registers.h"
#include "guest_ordering_table.h"
#include "horizontal_visibility_cull.h"
#include "lit_model_emitter.h"
#include "model_element.h"
#include "overlay_ground_gt3gt4.h"
#include "overlay_gt3gt4.h"
#include "state_producer.h"
#include "state_render_check.h"
#include "stub_game_runtime.h"
#include "sway_model_emitter.h"
#include "unlit_model_emitter.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <exception>
#include <lucent/log.h>
#include <memory>
#include <optional>

namespace {

// What the product keeps in Session::stateProducers.
psx::present::StateProducerTable stateProducers;

using psx::present::RecordKey;
using tomba::horizontal_cull::Visibility;
using tomba::render::LitModelEmitter;
using tomba::render::modelElement;
using tomba::render::ModelList;
using tomba::render::OrderingTable;
using tomba::render::PacketPool;
using tomba::render::SwayModelEmitter;
using tomba::render::UnlitModelEmitter;

constexpr GuestAddressRange kModeSlot{0x108F9Cu, 0x149F28u};
constexpr std::uint32_t kLitGt3 = 0x80129BACu;
constexpr std::uint32_t kLitGt4 = 0x8012A06Cu;
constexpr auto kRetail = LitModelEmitter::kRetailFlags;
constexpr std::uint32_t kPlainGt3 = 0x80140FBCu;
constexpr std::uint32_t kReturn = 0x80010100u;
constexpr std::uint32_t kStack = 0x801FFF00u;
constexpr std::uint32_t kPool = 0x801A0000u;
constexpr std::uint32_t kOt = 0x801B0000u;
constexpr std::uint32_t kRecords = 0x80190000u;
constexpr std::uint32_t kLcgSeed = 0x1F800080u;
constexpr std::uint32_t kLightPosition = 0x1F800160u;
constexpr std::uint32_t kA01ScrollU = 0x801388ECu;
constexpr std::uint32_t kA01ScrollV = 0x801388EEu;
constexpr std::uint32_t kLightIntensity = 0x800u;
constexpr std::int16_t kDepth = 1000;
constexpr Visibility kGuestFrame{{.left = 0, .right = 320}};
constexpr Visibility kCanvas16x9{{.left = -54, .right = 374}};

int failed = 0;
int checked = 0;

void check(bool condition, const char *name) {
  ++checked;
  if (!condition) {
    ++failed;
    lucent::error("model-emitters-test", "failed: {}", name);
  }
}

// Identity rotation at depth kDepth: a model point (x, y) projects to (160 + x, 120 + y).
void setUpGte(Core &core) {
  core.gte().bind();
  core.gte().writeControl(0u, 0x1000u);
  core.gte().writeControl(1u, 0u);
  core.gte().writeControl(2u, 0x1000u);
  core.gte().writeControl(3u, 0u);
  core.gte().writeControl(4u, 0x1000u);
  for (std::uint32_t reg = 5u; reg < 8u; ++reg) {
    core.gte().writeControl(reg, 0u);
  }
  core.gte().writeControl(psx::gte::kOfx, 160u << 16);
  core.gte().writeControl(psx::gte::kOfy, 120u << 16);
  core.gte().writeControl(psx::gte::kH, static_cast<std::uint32_t>(kDepth));
  core.gte().writeControl(29u, 0x555u); // ZSF3
  core.gte().writeControl(30u, 0x400u); // ZSF4
}

void beginFrame(Core &core) {
  PacketPool(core).setCursor(kPool);
  core.mem_w32(OrderingTable::kBasePointer, kOt);
  tomba::test::clearOrderingTable(core, kOt);
}

std::uint32_t xy(int x, int y) {
  return static_cast<std::uint16_t>(x) | (static_cast<std::uint32_t>(static_cast<std::uint16_t>(y)) << 16);
}

std::uint32_t zz(std::int16_t low, std::int16_t high) {
  return static_cast<std::uint16_t>(low) | (static_cast<std::uint32_t>(static_cast<std::uint16_t>(high)) << 16);
}

struct WriteGt3Args {
  std::uint32_t record;
  int x;
  std::uint32_t flag;
};
// A front-facing 20x20 triangle at model x (screen x - 160), flag byte 0 (average depth).
void writeGt3(Core &core, const WriteGt3Args &args) {
  auto [record, x, flag] = args;
  core.mem_w32(record + 0x00u, 0x34404040u);
  core.mem_w32(record + 0x04u, 0x00404040u | (flag << 24));
  core.mem_w32(record + 0x08u, 0x7FC00000u | 0x0810u);
  core.mem_w32(record + 0x0Cu, 0x00080000u | 0x0820u);
  core.mem_w32(record + 0x10u, xy(x, -10));
  core.mem_w32(record + 0x14u, zz(kDepth, kDepth));
  core.mem_w32(record + 0x18u, xy(x + 20, -10));
  core.mem_w32(record + 0x1Cu, xy(x, 10));
  core.mem_w32(record + 0x20u, zz(kDepth, 0x2830));
}

void writeGt4(Core &core, std::uint32_t record, int x) {
  core.mem_w32(record + 0x00u, 0x3C404040u);
  core.mem_w32(record + 0x04u, 0x00404040u);
  core.mem_w32(record + 0x08u, 0x7FC00810u);
  core.mem_w32(record + 0x0Cu, 0x00080820u);
  core.mem_w32(record + 0x10u, 0x28302830u);
  core.mem_w32(record + 0x14u, xy(x, -10));
  core.mem_w32(record + 0x18u, zz(kDepth, kDepth));
  core.mem_w32(record + 0x1Cu, xy(x + 20, -10));
  core.mem_w32(record + 0x20u, xy(x, 10));
  core.mem_w32(record + 0x24u, zz(kDepth, kDepth));
  core.mem_w32(record + 0x28u, xy(x + 20, 10));
}

struct ListArguments {
  std::uint32_t count;
  std::uint32_t intensity;
};
void arguments(Core &core, const ListArguments &list) {
  const auto [count, intensity] = list;
  core.r[4] = kRecords;
  core.r[5] = kOt;
  core.r[6] = count;
  core.r[7] = intensity;
  core.r[29] = kStack;
  core.r[31] = kReturn;
}

bool call(Core &core, std::uint32_t entry) {
  return psx::cpu::dispatchGuest(core, entry, psx::cpu::ExecutionBudget::fromCycles(1000000u)).returned();
}

struct Pair {
  const char *overlay;
  std::uint32_t gt3;
  std::uint32_t gt4;
};

// A08's lit pair, SOP's unlit pair, A01's sway and cue emitters.
constexpr Pair kPairs[] = {
    {"A08", kLitGt3, kLitGt4}, {"SOP", 0x801099B4u, 0x80109C80u}, {"A01", 0x80130838u, 0x8013000Cu}};

// Through each override at 4:3: one packet per visible record, keyed by the list and its index.
void testKeys(const tomba::native::NativeOverrideCatalog &natives,
              Core &core,
              std::optional<psx::cpu::ImageIdentity> &mode) {
  for (const Pair &pair : kPairs) {
    natives.activateOverlay(core, mode, pair.overlay, kModeSlot);
    beginFrame(core);
    writeGt3(core, {.record = kRecords, .x = 0, .flag = 0u});
    writeGt3(core, {.record = kRecords + 0x24u, .x = -400, .flag = 0u});
    writeGt3(core, {.record = kRecords + 0x48u, .x = 40, .flag = 0u});
    arguments(core, {.count = 3u, .intensity = kLightIntensity});
    check(call(core, pair.gt3), "the GT3 emitter returns");
    check(core.r[2] == kRecords + 3u * 0x24u && PacketPool(core).cursor() == kPool + 2u * 0x28u,
          "two of three triangles are on screen; v0 is past the list");
    check(core.emission.identityFor(kPool) == RecordKey{pair.gt3, kRecords, modelElement(ModelList::Gt3, 0u), 0u} &&
              core.emission.identityFor(kPool + 0x28u) ==
                  RecordKey{pair.gt3, kRecords, modelElement(ModelList::Gt3, 2u), 0u},
          "each packet is keyed by its record's index, whatever was culled before it");
    check(core.r[29] == kStack, "sp is restored");

    beginFrame(core);
    writeGt4(core, kRecords, 0);
    writeGt4(core, kRecords + 0x2Cu, 20);
    arguments(core, {.count = 2u, .intensity = kLightIntensity});
    check(call(core, pair.gt4), "the GT4 emitter returns");
    check(PacketPool(core).cursor() == kPool + 2u * 0x34u &&
              core.emission.identityFor(kPool + 0x34u) ==
                  RecordKey{pair.gt4, kRecords, modelElement(ModelList::Gt4, 1u), 0u},
          "each GT4 packet is keyed by its record's index");
    check(core.mem_r32(kPool) >> 24 == 12u && (core.mem_r32(kPool + 4u) >> 24) == 0x3Cu,
          "a GT4 packet is 12 words with the record's GP0 code");
  }

  natives.activateOverlay(core, mode, "A08", kModeSlot);
  beginFrame(core);
  writeGt3(core, {.record = kRecords, .x = 0, .flag = 0u});
  arguments(core, {.count = 1u, .intensity = kLightIntensity});
  check(call(core, kLitGt3), "the lit GT3 emitter returns");
  check(core.mem_r32(kStack - 0x40u + 0x3Cu) == kReturn && core.mem_r32(kStack + 4u) == kOt &&
            core.mem_r32(kStack + 0xCu) == kLightIntensity,
        "the lit guest frame spills ra and the OT and intensity arguments");
}

using Emit = void (*)(Core &, const Visibility &);

void litGt3(Core &core, const Visibility &visible) {
  LitModelEmitter::gt3(core, visible, kRetail);
}
void litGt4(Core &core, const Visibility &visible) {
  LitModelEmitter::gt4(core, visible, kRetail);
}
void plainGt3(Core &core, const Visibility &visible) {
  tomba::OverlayGt3Gt4::gt3(core, tomba::OverlayGt3Gt4::kA08Scroll, visible);
}
void sopGt3(Core &core, const Visibility &visible) {
  UnlitModelEmitter::gt3(core, visible, UnlitModelEmitter::kSop);
}
void residentGt4(Core &core, const Visibility &visible) {
  UnlitModelEmitter::gt4(core, visible, UnlitModelEmitter::kResident);
}

struct MarginCase {
  const char *name;
  Emit emit;
  bool quad;
};

constexpr MarginCase kMarginCases[] = {{"lit GT3", &litGt3, false},
                                       {"lit GT4", &litGt4, true},
                                       {"A08 plain GT3", &plainGt3, false},
                                       {"SOP GT3", &sopGt3, false},
                                       {"resident GT4", &residentGt4, true},
                                       {"A01 sway GT3", &SwayModelEmitter::swayGt3, false},
                                       {"A01 scroll GT4", &SwayModelEmitter::scrollGt4, true},
                                       {"A01 cue GT3", &SwayModelEmitter::cueGt3, false},
                                       {"A01 cue GT4", &SwayModelEmitter::cueGt4, true}};

// A triangle wholly in the left margin and a quad wholly in the right: dropped at 4:3, drawn on the 16:9
// record canvas.
void testMarginCull(Core &core) {
  const psx::present::EmissionScope::Guard producer(core.emission, kLitGt3, kRecords, 0u);
  for (const MarginCase &margin : kMarginCases) {
    for (const bool canvas : {false, true}) {
      beginFrame(core);
      if (margin.quad) {
        writeGt4(core, kRecords, 180);
      } else {
        writeGt3(core, {.record = kRecords, .x = -200, .flag = 0u});
      }
      arguments(core, {.count = 1u, .intensity = kLightIntensity});
      margin.emit(core, canvas ? kCanvas16x9 : kGuestFrame);
      const std::uint32_t drawn = canvas ? (margin.quad ? 0x34u : 0x28u) : 0u;
      const bool kept = PacketPool(core).cursor() == kPool + drawn;
      check(kept && core.r[29] == kStack,
            canvas ? "the record canvas keeps a primitive in the margin" : "4:3 drops a primitive outside the screen");
      if (!kept) {
        lucent::error("model-emitters-test", "{} on the {}", margin.name, canvas ? "16:9 canvas" : "4:3 frame");
      }
    }
  }
}

// A08's plain pair adds the scroll word to the packet's UVs when the record's flag byte has bit 2.
void testScroll(Core &core) {
  const psx::present::EmissionScope::Guard producer(core.emission, kPlainGt3, kRecords, 0u);
  core.mem_w16(tomba::OverlayGt3Gt4::kA08Scroll, 3u);
  for (const std::uint32_t scroll : {tomba::OverlayGt3Gt4::kNoScroll, tomba::OverlayGt3Gt4::kA08Scroll}) {
    beginFrame(core);
    writeGt3(core, {.record = kRecords, .x = 0, .flag = 4u});
    arguments(core, {.count = 1u, .intensity = 0u});
    tomba::OverlayGt3Gt4::gt3(core, scroll, kGuestFrame);
    const std::uint16_t uv0 = core.mem_r16(kPool + 12u);
    const std::uint16_t uv1 = core.mem_r16(kPool + 24u);
    const std::uint16_t uv2 = core.mem_r16(kPool + 36u);
    if (scroll == tomba::OverlayGt3Gt4::kNoScroll) {
      check(uv0 == 0x0810u && uv1 == 0x0820u && uv2 == 0x2830u, "A00's body leaves the UVs as recorded");
    } else {
      check(uv0 == 0x0816u && uv1 == 0x0823u && uv2 == 0x2830u, "A08's GT3 scrolls uv0 twice and uv1 once");
    }
  }
  beginFrame(core);
  writeGt3(core, {.record = kRecords, .x = 0, .flag = 0u});
  arguments(core, {.count = 1u, .intensity = 0u});
  tomba::OverlayGt3Gt4::gt3(core, tomba::OverlayGt3Gt4::kA08Scroll, kGuestFrame);
  check(core.mem_r16(kPool + 12u) == 0x0810u, "a record without the scroll flag keeps its UVs");
}

// The state renders of the list emitters: a list drawn through the shipping override, its state collected by
// the scope serial its packets carry, and the render run over host memory.
constexpr std::uint32_t kStatePool = 0x001A0000u;
constexpr std::uint32_t kStateRecords4 = 0x80191000u;

struct StateFamily {
  const char *name;
  const char *overlay;
  std::uint32_t gt3;
  std::uint32_t gt4;
  std::array<std::uint32_t, 3> flags; // the triangles' flag bytes: average, then the other depth modes
};

constexpr StateFamily kStateFamilies[] = {{"SOP unlit", "SOP", 0x801099B4u, 0x80109C80u, {0u, 1u, 2u}},
                                          {"A08 plain", "A08", 0x80140FBCu, 0x801411D8u, {0u, 3u, 4u}},
                                          {"A00 ground", "A00", 0x8013FB88u, 0x8013FE58u, {0u, 1u, 2u}},
                                          {"A08 lit", "A08", 0x80129BACu, 0x8012A06Cu, {0u, 1u, 2u}},
                                          {"A01 cue", "A01", 0x8012F8D8u, 0x8013000Cu, {0u, 0x14u, 0x11u}},
                                          {"A01 sway", "A01", 0x80130838u, 0x80130D9Cu, {0u, 0x10u, 0x11u}}};

// Three triangles and two quads through the family's emitters, the camera `shift` right.
psx::present::FrameRecord drawStateFrame(Core &core, const StateFamily &family, std::uint32_t shift) {
  beginFrame(core);
  PacketPool(core).setCursor(kStatePool);
  core.gte().writeControl(5u, shift);
  core.mem_w16(tomba::OverlayGt3Gt4::kA08Scroll, 3u);
  core.mem_w8(kA01ScrollU, 5u);
  core.mem_w8(kA01ScrollV, 7u);
  core.mem_w32(kLcgSeed, 0x12345u);
  core.mem_w16(kLightPosition, 40u);
  core.mem_w16(kLightPosition + 2u, 0xFFECu);
  core.mem_w16(kLightPosition + 4u, 10u);
  writeGt3(core, {.record = kRecords, .x = 0, .flag = family.flags[0]});
  writeGt3(core, {.record = kRecords + 0x24u, .x = 30, .flag = family.flags[1]});
  writeGt3(core, {.record = kRecords + 0x48u, .x = -30, .flag = family.flags[2]});
  arguments(core, {.count = 3u, .intensity = 0u});
  check(call(core, family.gt3), "the GT3 emitter returns");
  writeGt4(core, kStateRecords4, 0);
  writeGt4(core, kStateRecords4 + 0x2Cu, 40);
  core.mem_w32(kLcgSeed, 0x6789Au);
  arguments(core, {.count = 2u, .intensity = 0u});
  core.r[4] = kStateRecords4;
  check(call(core, family.gt4), "the GT4 emitter returns");
  return tomba::test::submittedFrame(core, kOt);
}

std::vector<psx::present::DrawPrimitive> composedOf(const tomba::test::ComposeArgs &args, psx::present::ObjectId id) {
  check(stateProducers.find(id.producer) != nullptr && args.to.find(id) && args.from.find(id),
        "the emitter has a render and a state in both frames");
  return tomba::test::composedPrimitives(stateProducers, args, id);
}

void testStateRenderExact(Core &core, const StateFamily &family) {
  const psx::present::FrameRecord frame = drawStateFrame(core, family, 0u);
  const psx::present::FrameState state = core.frameStates.collect(frame);
  const auto guestGt3 = tomba::test::primitivesOf(frame, family.gt3);
  const auto guestGt4 = tomba::test::primitivesOf(frame, family.gt4);
  check(guestGt3.size() == 3u && guestGt4.size() == 2u, family.name);

  const tomba::test::ComposeArgs exact{.shown = frame, .from = state, .to = state};
  check(tomba::test::sameFrame(composedOf(exact, {family.gt3, kRecords}), guestGt3),
        "t = 1 reproduces the GT3 packets the guest body wrote, bucket and all");
  check(tomba::test::sameFrame(composedOf(exact, {family.gt4, kStateRecords4}), guestGt4),
        "t = 1 reproduces the GT4 packets the guest body wrote, bucket and all");

  // The render reads the saved call only: scramble the records, the pool and the GTE it came from.
  for (std::uint32_t offset = 0; offset < 0x100u; offset += 4u) {
    core.mem_w32(kRecords + offset, 0xA5A5A5A5u);
    core.mem_w32(kStateRecords4 + offset, 0x5A5A5A5Au);
  }
  for (std::uint32_t offset = 0; offset < 0x200u; offset += 4u) {
    core.mem_w32(kStatePool + offset, 0xDEADBEEFu);
  }
  core.mem_w16(tomba::OverlayGt3Gt4::kA08Scroll, 0x1234u);
  core.mem_w8(kA01ScrollU, 0x33u);
  core.mem_w32(kLcgSeed, 0xFFFFFFFFu);
  core.mem_w16(kLightPosition, 1000u);
  setUpGte(core);
  core.gte().writeControl(5u, 777u);
  check(tomba::test::sameFrame(composedOf(exact, {family.gt3, kRecords}), guestGt3),
        "the render does not depend on the guest memory the call read");
  setUpGte(core);
}

void testStateRenderInBetween(Core &core, const StateFamily &family) {
  const psx::present::FrameRecord before = drawStateFrame(core, family, 0u);
  const psx::present::FrameState earlier = core.frameStates.collect(before);
  const psx::present::FrameRecord after = drawStateFrame(core, family, 40u);
  const psx::present::FrameState later = core.frameStates.collect(after);
  const psx::present::FrameRecord midway = drawStateFrame(core, family, 20u);

  const tomba::test::ComposeArgs halfway{.shown = after, .from = earlier, .to = later, .t = 0.5f};
  const auto gt3 = composedOf(halfway, {family.gt3, kRecords});
  const auto gt4 = composedOf(halfway, {family.gt4, kStateRecords4});
  check(tomba::test::sameFrame(gt3, tomba::test::primitivesOf(midway, family.gt3)) &&
            tomba::test::sameFrame(gt4, tomba::test::primitivesOf(midway, family.gt4)),
        "t = 0.5 draws what the body draws with the camera halfway between the two frames");
  check(!tomba::test::sameFrame(gt3, tomba::test::primitivesOf(before, family.gt3)) &&
            !tomba::test::sameFrame(gt3, tomba::test::primitivesOf(after, family.gt3)),
        "the in-between differs from both frames");
  setUpGte(core);
}

void testStateRenders(const tomba::native::NativeOverrideCatalog &natives,
                      Core &core,
                      std::optional<psx::cpu::ImageIdentity> &mode) {
  for (const StateFamily &family : kStateFamilies) {
    if (family.overlay != nullptr) {
      natives.activateOverlay(core, mode, family.overlay, kModeSlot);
    }
    testStateRenderExact(core, family);
    testStateRenderInBetween(core, family);
  }
}

} // namespace

namespace {

int runTests() {
  tomba::native::NativeOverrideCatalog natives;
  psx::test::StubGameRuntime runtime;
  auto game = std::make_unique<Game>(runtime, psx::config::Config{});
  Core &core = game->core;
  game->mods.aspect = ASPECT_4_3; // the record path, whatever the local settings say
  setUpGte(core);
  LitModelEmitter::registerOverrides(natives);
  UnlitModelEmitter::registerOverrides(natives);
  SwayModelEmitter::registerOverrides(natives);
  tomba::OverlayGt3Gt4::registerOverrides(natives);
  tomba::OverlayGroundGt3Gt4::registerOverrides(natives);
  UnlitModelEmitter::registerStateRenders(stateProducers, core);
  tomba::OverlayGt3Gt4::registerStateRenders(stateProducers, core);
  SwayModelEmitter::registerStateRenders(stateProducers, core);
  LitModelEmitter::registerStateRenders(stateProducers, core);
  tomba::OverlayGroundGt3Gt4::registerStateRenders(stateProducers, core);
  std::optional<psx::cpu::ImageIdentity> mode;

  testKeys(natives, core, mode);
  testMarginCull(core);
  testScroll(core);
  testStateRenders(natives, core, mode);
  lucent::info("model-emitters-test", "checked={} failed={}", checked, failed);
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
