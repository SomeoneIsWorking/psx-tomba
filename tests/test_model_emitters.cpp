// The native model emitters (LitModelEmitter, UnlitModelEmitter, SwayModelEmitter, OverlayGt3Gt4's A08
// pair): their keys, their screen cull across the record canvas, and A08's texture scroll, through the
// shipping overrides.
#include "core/overrides/native_override_catalog.h"
#include "game.h"
#include "gte_registers.h"
#include "guest_ordering_table.h"
#include "horizontal_visibility_cull.h"
#include "hw_bind.h"
#include "lit_model_emitter.h"
#include "model_element.h"
#include "overlay_gt3gt4.h"
#include "stub_runtime.h"
#include "sway_model_emitter.h"
#include "unlit_model_emitter.h"

#include <cstdint>
#include <lucent/log.h>
#include <memory>
#include <optional>

namespace {

using psx::present::RecordKey;
using tomba2::horizontal_cull::Visibility;
using tomba2::render::LitModelEmitter;
using tomba2::render::modelElement;
using tomba2::render::ModelList;
using tomba2::render::OrderingTable;
using tomba2::render::PacketPool;
using tomba2::render::SwayModelEmitter;
using tomba2::render::UnlitModelEmitter;

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
constexpr std::uint32_t kChainEnd = 0x00FFFFFFu;
constexpr std::uint32_t kBuckets = 0x800u;
constexpr std::uint32_t kLightIntensity = 0x800u;
constexpr std::int16_t kDepth = 1000;
const Visibility kGuestFrame{0, 320};
const Visibility kCanvas16x9{-54, 374};

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
  gte_bind(&core);
  gte_write_ctrl(0u, 0x1000u);
  gte_write_ctrl(1u, 0u);
  gte_write_ctrl(2u, 0x1000u);
  gte_write_ctrl(3u, 0u);
  gte_write_ctrl(4u, 0x1000u);
  for (std::uint32_t reg = 5u; reg < 8u; ++reg) {
    gte_write_ctrl(reg, 0u);
  }
  gte_write_ctrl(tomba2::gte::kOfx, 160u << 16);
  gte_write_ctrl(tomba2::gte::kOfy, 120u << 16);
  gte_write_ctrl(tomba2::gte::kH, static_cast<std::uint32_t>(kDepth));
  gte_write_ctrl(29u, 0x555u); // ZSF3
  gte_write_ctrl(30u, 0x400u); // ZSF4
}

void beginFrame(Core &core) {
  PacketPool(core).setCursor(kPool);
  core.mem_w32(OrderingTable::kBasePointer, kOt);
  for (std::uint32_t bucket = 0; bucket < kBuckets; ++bucket) {
    core.mem_w32(kOt + bucket * 4u, kChainEnd);
  }
}

std::uint32_t xy(int x, int y) {
  return static_cast<std::uint16_t>(x) | (static_cast<std::uint32_t>(static_cast<std::uint16_t>(y)) << 16);
}

std::uint32_t zz(std::int16_t low, std::int16_t high) {
  return static_cast<std::uint16_t>(low) | (static_cast<std::uint32_t>(static_cast<std::uint16_t>(high)) << 16);
}

// A front-facing 20x20 triangle at model x (screen x - 160), flag byte 0 (average depth).
void writeGt3(Core &core, std::uint32_t record, int x, std::uint32_t flag) {
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

void arguments(Core &core, std::uint32_t count, std::uint32_t intensity) {
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
void testKeys(Core &core, std::optional<psx::cpu::ImageIdentity> &mode) {
  for (const Pair &pair : kPairs) {
    tomba::native::activateOverlay(core, mode, pair.overlay, kModeSlot);
    beginFrame(core);
    writeGt3(core, kRecords, 0, 0u);
    writeGt3(core, kRecords + 0x24u, -400, 0u);
    writeGt3(core, kRecords + 0x48u, 40, 0u);
    arguments(core, 3u, kLightIntensity);
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
    arguments(core, 2u, kLightIntensity);
    check(call(core, pair.gt4), "the GT4 emitter returns");
    check(PacketPool(core).cursor() == kPool + 2u * 0x34u &&
              core.emission.identityFor(kPool + 0x34u) ==
                  RecordKey{pair.gt4, kRecords, modelElement(ModelList::Gt4, 1u), 0u},
          "each GT4 packet is keyed by its record's index");
    check(core.mem_r32(kPool) >> 24 == 12u && (core.mem_r32(kPool + 4u) >> 24) == 0x3Cu,
          "a GT4 packet is 12 words with the record's GP0 code");
  }

  tomba::native::activateOverlay(core, mode, "A08", kModeSlot);
  beginFrame(core);
  writeGt3(core, kRecords, 0, 0u);
  arguments(core, 1u, kLightIntensity);
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
  OverlayGt3Gt4::gt3(core, OverlayGt3Gt4::kA08Scroll, visible);
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
        writeGt3(core, kRecords, -200, 0u);
      }
      arguments(core, 1u, kLightIntensity);
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
  core.mem_w16(OverlayGt3Gt4::kA08Scroll, 3u);
  for (const std::uint32_t scroll : {OverlayGt3Gt4::kNoScroll, OverlayGt3Gt4::kA08Scroll}) {
    beginFrame(core);
    writeGt3(core, kRecords, 0, 4u);
    arguments(core, 1u, 0u);
    OverlayGt3Gt4::gt3(core, scroll, kGuestFrame);
    const std::uint16_t uv0 = core.mem_r16(kPool + 12u);
    const std::uint16_t uv1 = core.mem_r16(kPool + 24u);
    const std::uint16_t uv2 = core.mem_r16(kPool + 36u);
    if (scroll == OverlayGt3Gt4::kNoScroll) {
      check(uv0 == 0x0810u && uv1 == 0x0820u && uv2 == 0x2830u, "A00's body leaves the UVs as recorded");
    } else {
      check(uv0 == 0x0816u && uv1 == 0x0823u && uv2 == 0x2830u, "A08's GT3 scrolls uv0 twice and uv1 once");
    }
  }
  beginFrame(core);
  writeGt3(core, kRecords, 0, 0u);
  arguments(core, 1u, 0u);
  OverlayGt3Gt4::gt3(core, OverlayGt3Gt4::kA08Scroll, kGuestFrame);
  check(core.mem_r16(kPool + 12u) == 0x0810u, "a record without the scroll flag keeps its UVs");
}

} // namespace

int main() {
  tomba::test::StubRuntime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  game->mods.aspect = ASPECT_4_3; // the record path, whatever the local settings say
  setUpGte(core);
  LitModelEmitter::registerOverrides();
  UnlitModelEmitter::registerOverrides();
  SwayModelEmitter::registerOverrides();
  OverlayGt3Gt4::registerOverrides(game.get());
  std::optional<psx::cpu::ImageIdentity> mode;

  testKeys(core, mode);
  testMarginCull(core);
  testScroll(core);
  lucent::info("model-emitters-test", "checked={} failed={}", checked, failed);
  return failed == 0 ? 0 : 1;
}
