// The issue-0033 writers through the shipping overrides and composeFrame: at t = 1 the composed frame is the
// one the guest drew, at t = 0.5 it is the frame drawn halfway between the two saved states.
#include "compose_check.h"
#include "core/entry/runtime.h"
#include "core/overrides/native_override_catalog.h"
#include "game.h"
#include "gte_registers.h"
#include "math/gte_math.h"
#include "render/drop_shadow.h"
#include "render/guest_ordering_table.h"
#include "render/obj_model_view.h"
#include "render/packed_mesh.h"
#include "render/screen_fade.h"
#include "state_producer.h"
#include "state_render_check.h"
#include "ui/ui_sprite.h"

#include <exception>
#include <lucent/log.h>
#include <memory>
#include <vector>

namespace {

constexpr GuestAddressRange kResidentText{0x10000u, 0xBE800u};
constexpr std::uint32_t kPool = 0x001A0000u; // retail keeps the cursor as a 24-bit address
constexpr std::uint32_t kOt = 0x801B0000u;
constexpr std::uint32_t kStack = 0x801FFF00u;
constexpr std::uint32_t kCallSite = 0x80106A10u;
constexpr auto kBudget = psx::cpu::ExecutionBudget::fromCycles(5000000u);

// SetDrawMode(p, 0, 0, tpage, 0) as the writers call it: the E1 word and an empty texture window.
constexpr std::uint32_t kSetDrawMode = 0x80083DE0u;
constexpr std::uint32_t kSetDrawModeBody[] = {
    0x3C08E100u, // lui t0, 0xE100
    0x30E909FFu, // andi t1, a3, 0x9FF
    0x01094025u, // or t0, t0, t1
    0xAC880004u, // sw t0, 4(a0)
    0x03E00008u, // jr ra
    0xAC800008u, // sw zero, 8(a0)
};

constexpr std::uint32_t kSinCosTable = 0x800A6490u; // angle 0: cos 1.0, sin 0

int failed = 0;
int checked = 0;

void check(bool condition, const char *name) {
  ++checked;
  if (!condition) {
    ++failed;
    lucent::error("writer-producers-test", "failed: {}", name);
  }
}

// Draws `from` and `to`, and the frame halfway between them; composeFrame at t = 1 and 0.5 must give the last two.
using tomba::test::ComposeHarness;
using tomba::test::DrawnFrame;

void checkComposes(const ComposeHarness &harness,
                   const char *writer,
                   const DrawnFrame &from,
                   const DrawnFrame &to,
                   const DrawnFrame &midway) {
  lucent::info("writer-producers-test", "{}: {} primitives drawn", writer, harness.primitives(to.shown).size());
  check(harness.composesAsDrawn(from, to), "t = 1 composes the frame the guest drew");
  check(harness.composesBetween(from, to, midway.shown),
        "t = 0.5 composes the frame drawn halfway between the two states");
}

bool call(Core &core, std::uint32_t entry) {
  core.r[29] = kStack;
  core.r[31] = kCallSite;
  return psx::cpu::dispatchGuest(core, entry, kBudget).returned();
}

// Identity rotation, translation (x, 0, 2000), the retail projection.
void setCamera(Core &core, std::uint32_t x) {
  constexpr std::uint32_t kUnit = 0x1000u;
  const std::uint32_t control[8] = {kUnit, 0u, kUnit, 0u, kUnit, x, 0u, 2000u};
  for (std::uint32_t reg = 0; reg < 8u; ++reg) {
    core.gte().writeControl(reg, control[reg]);
  }
  core.gte().writeControl(psx::gte::kOfx, 160u << 16);
  core.gte().writeControl(psx::gte::kOfy, 120u << 16);
  core.gte().writeControl(psx::gte::kH, 350u);
  core.gte().writeControl(29u, 0x155u); // ZSF3
  core.gte().writeControl(30u, 0x100u); // ZSF4
}

void testFade(Core &core, ComposeHarness &harness) {
  const auto draw = [&](std::uint32_t colour) {
    return harness.frame([&] {
      core.r[4] = colour;
      core.r[5] = tomba::ScreenFade::kSubtractive;
      core.r[6] = 2u;
      check(call(core, tomba::ScreenFade::kLeaf), "the fade returns");
    });
  };
  const auto from = draw(0x204060u);
  const auto to = draw(0x6080A0u);
  checkComposes(harness, "fade", from, to, draw(0x406080u));
}

// A two-piece definition at `defs`: index 0 names the pieces that follow it.
constexpr std::uint32_t kPlacement = 0x80197000u;
constexpr std::uint32_t kIndex = 0x80197010u;
constexpr std::uint32_t kAttrs = 0x80197020u;
constexpr std::uint32_t kDefs = 0x80197100u;

// A sprite writer call: the writer, the attribute flags and what the log names it.
struct SpriteCall {
  std::uint32_t entry;
  std::uint8_t flags;
  const char *name;
};

void writeSprite(Core &core, const SpriteCall &call, std::uint32_t x) {
  core.mem_w16(kPlacement, static_cast<std::uint16_t>(x));
  core.mem_w16(kPlacement + 2u, 60u);
  core.mem_w32(kPlacement + 4u, 0u);
  core.mem_w16(kIndex, 0u);
  core.mem_w8(kAttrs, call.flags);
  core.mem_w8(kAttrs + 1u, 3u);
  core.mem_w16(kAttrs + 2u, 0u);
  core.mem_w16(kDefs, 2u);
  core.mem_w16(kDefs + 2u, 0x10u);
  for (std::uint32_t piece = 0; piece < 2u; ++piece) {
    const std::uint32_t at = kDefs + 0x10u + piece * 16u;
    core.mem_w32(at, 0x7C000000u | (piece * 16u)); // uv0, CLUT
    core.mem_w32(at + 4u, 0x00080010u);            // uv1, texture page 8
    core.mem_w16(at + 8u, 0x1000u);
    core.mem_w8(at + 0xAu, 16u);
    core.mem_w8(at + 0xBu, 12u);
    core.mem_w16(at + 0xCu, 0x1010u);
    core.mem_w8(at + 0xEu, static_cast<std::uint8_t>(piece * 16u));
    core.mem_w8(at + 0xFu, 0xFEu);
  }
  core.r[4] = kPlacement;
  core.r[5] = kIndex;
  core.r[6] = kDefs;
  core.r[7] = kAttrs;
}

void testSprites(Core &core, ComposeHarness &harness, const SpriteCall &sprite) {
  const auto draw = [&](std::uint32_t x) {
    return harness.frame([&] {
      writeSprite(core, sprite, x);
      check(call(core, sprite.entry), "the sprite writer returns");
    });
  };
  const auto from = draw(100u);
  const auto to = draw(140u);
  checkComposes(harness, sprite.name, from, to, draw(120u));
}

// Three records of the packed-mesh format, the last ending the list.
void writeMesh(Core &core, std::uint32_t list) {
  for (std::uint32_t record = 0; record < 3u; ++record) {
    const std::uint32_t at = list + record * tomba::render::PackedMesh::kRecordBytes;
    core.mem_w32(at, 0x7C400000u);                                   // uv0, CLUT
    core.mem_w32(at + 4u, record == 2u ? 0x80081010u : 0x40081010u); // uv1, page 8; semi; last
    core.mem_w32(at + 8u, 0x30303030u);
    core.mem_w32(at + 0xCu, 0x00808080u);
    core.mem_w32(at + 0x10u, 0x00406080u);
    core.mem_w32(at + 0x14u, 0x00804060u);
    core.mem_w32(at + 0x18u, 0x00202020u);
    const std::uint8_t x = static_cast<std::uint8_t>(1u + record);
    for (const std::uint32_t z : {0x0Fu, 0x13u, 0x17u, 0x1Bu}) {
      core.mem_w8(at + z, 1u);
    }
    core.mem_w8(at + 0x1Cu, x);
    core.mem_w8(at + 0x1Du, static_cast<std::uint8_t>(x + 1u));
    core.mem_w8(at + 0x20u, x);
    core.mem_w8(at + 0x21u, static_cast<std::uint8_t>(x + 1u));
    core.mem_w8(at + 0x1Eu, 0u);
    core.mem_w8(at + 0x1Fu, 0u);
    core.mem_w8(at + 0x22u, 1u);
    core.mem_w8(at + 0x23u, 1u);
  }
}

void testPackedMesh(Core &core, ComposeHarness &harness) {
  constexpr std::uint32_t kList = 0x80198000u;
  const auto draw = [&](std::uint32_t x) {
    return harness.frame([&] {
      setCamera(core, x);
      writeMesh(core, kList);
      core.mem_w32(tomba::render::PackedMesh::kDepthCue, 0x400u);
      core.r[4] = kList;
      core.r[5] = 1u;
      core.r[6] = 0xFFF0u;
      core.r[7] = 4u;
      check(call(core, tomba::render::PackedMesh::kWriter), "the mesh writer returns");
    });
  };
  const auto from = draw(0u);
  const auto to = draw(200u);
  checkComposes(harness, "packed mesh", from, to, draw(100u));
}

void testDropShadow(Core &core, ComposeHarness &harness) {
  constexpr std::uint32_t kNode = 0x80199000u;
  constexpr std::uint32_t kCamera = 0x1F8000F8u;
  const auto draw = [&](std::int16_t x) {
    return harness.frame([&] {
      setCamera(core, 0u);
      writeMesh(core, tomba::render::DropShadow::kMesh);
      for (std::uint32_t word = 0; word < 5u; ++word) {
        core.mem_w32(kCamera + word * 4u, word % 2u == 0u ? 0x1000u : 0u);
      }
      core.mem_w32(0x1F80010Cu, 0u);
      core.mem_w32(0x1F800110u, 0u);
      core.mem_w32(0x1F800114u, 0x800u);
      core.mem_w8(tomba::render::DropShadow::kScale + 1u, 0x20u);
      core.mem_w16(kNode + 0x40u, 0x200u);
      core.mem_w16(kNode + 0x42u, 0x200u);
      core.mem_w32(kNode + 0x48u, static_cast<std::uint16_t>(x));
      core.mem_w32(kNode + 0x4Cu, 0u);
      core.mem_w16(kNode + 0x4Eu, 0x40u);
      core.mem_w16(kNode + 0x50u, 0x100u);
      core.mem_w32(kNode + 0x54u, 0u);
      core.mem_w32(kNode + 0x58u, 0u);
      core.r[4] = kNode;
      check(call(core, tomba::render::DropShadow::kDraw), "the drop shadow returns");
    });
  };
  const auto from = draw(0);
  const auto to = draw(200);
  checkComposes(harness, "drop shadow", from, to, draw(100));
}

int runTests() {
  tomba::Runtime runtime;
  auto game = std::make_unique<Game>(runtime, psx::config::Config{});
  Core &core = game->core;
  game->mods.aspect = ASPECT_4_3;
  core.gte().bind();
  for (std::uint32_t word = 0; word < std::size(kSetDrawModeBody); ++word) {
    core.mem_w32(kSetDrawMode + word * 4u, kSetDrawModeBody[word]);
  }
  core.mem_w32(kSinCosTable, 0x10000000u);

  tomba::native::NativeOverrideCatalog natives;
  tomba::ScreenFade::registerOverrides(natives);
  tomba::UiSprite::registerOverrides(natives);
  tomba::render::PackedMesh::registerOverrides(natives);
  tomba::render::DropShadow::registerOverrides(natives);
  tomba::ObjModelView::registerOverrides(natives);
  tomba::mathOf(&core).registerOverrides(natives);
  const auto resident = core.imageCatalog().activate("resident", kResidentText, 1u);
  natives.bindResident(core, resident, kResidentText);

  ComposeHarness harness(core, kOt, kPool);
  tomba::ScreenFade::registerStateRenders(harness.renders());
  tomba::UiSprite::registerStateRenders(harness.renders());
  tomba::render::PackedMesh::registerStateRenders(harness.renders(), core);

  testFade(core, harness);
  testSprites(core, harness, {.entry = tomba::UiSprite::kQuads, .flags = 0u, .name = "quads"});
  testSprites(core, harness, {.entry = tomba::UiSprite::kQuads, .flags = 3u, .name = "quads mirrored"});
  testSprites(core, harness, {.entry = tomba::UiSprite::kCompose, .flags = 0x40u, .name = "compose"});
  testPackedMesh(core, harness);
  testDropShadow(core, harness);

  lucent::info("writer-producers-test", "checked={} failed={}", checked, failed);
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
