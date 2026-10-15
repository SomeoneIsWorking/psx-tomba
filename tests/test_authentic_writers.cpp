// The issue-0033 writer ports against the guest bodies of the authenticated MAIN.EXE: FUN_8007E1B8 (UiSprite::quads),
// FUN_80027768 (PackedMesh) and FUN_8002AB5C (DropShadow) write the guest's bytes from random inputs; the packed
// mesh's render at t = 1 draws the packets it wrote; billboardEmit's state composes through composeFrame.
#include "authenticated_image.h"
#include "compose_check.h"
#include "core/entry/runtime.h"
#include "core/entry/session.h"
#include "game.h"
#include "gte_registers.h"
#include "guest_snapshot.h"
#include "psx_exe_image.h"
#include "render/billboard_state.h"
#include "render/drop_shadow.h"
#include "render/guest_ordering_table.h"
#include "render/packed_mesh.h"
#include "state_render_check.h"
#include "ui/ui_sprite.h"

#include <cstdint>
#include <exception>
#include <lucent/log.h>
#include <memory>
#include <random>

namespace {

using tomba::test::capture;
using tomba::test::firstDifference;
using tomba::test::GuestSnapshot;
using tomba::test::restore;

constexpr std::uint32_t kOt = 0x801B0000u;
constexpr std::uint32_t kPool = 0x001A0000u; // retail keeps the cursor as a 24-bit address
constexpr std::uint32_t kStack = 0x801FFF00u;
constexpr std::uint32_t kReturn = 0x80010100u;
constexpr std::uint32_t kData = 0x80190000u;
constexpr int kCases = 300;
constexpr auto kBudget = psx::cpu::ExecutionBudget::fromCycles(50000000u);

int failed = 0;

void fail(const char *writer, int run, const GuestSnapshot &guest, const GuestSnapshot &native) {
  ++failed;
  const auto ram = firstDifference(guest.ram.data(), native.ram.data(), guest.ram.size());
  const auto scratch = firstDifference(guest.scratch.data(), native.scratch.data(), guest.scratch.size());
  lucent::error("authentic-writers",
                "{} case {}: first RAM diff 0x{:08X}, first scratchpad diff 0x{:08X}, v0 0x{:08X}/0x{:08X}, sp "
                "0x{:08X}/0x{:08X}",
                writer,
                run,
                0x80000000u + ram.value_or(0x7FFFFFFFu),
                0x1F800000u + scratch.value_or(0xFFFFFu),
                guest.r[2],
                native.r[2],
                guest.r[29],
                native.r[29]);
}

class Inputs {
public:
  explicit Inputs(std::uint32_t seed) : rng_(seed) {}
  std::uint32_t word() {
    return std::uniform_int_distribution<std::uint32_t>()(rng_);
  }
  std::int32_t range(std::int32_t low, std::int32_t high) {
    return std::uniform_int_distribution<std::int32_t>(low, high)(rng_);
  }

private:
  std::mt19937 rng_;
};

void clearFrame(Core &core, Inputs &inputs) {
  core.mem_w32(tomba::render::OrderingTable::kBasePointer, kOt);
  tomba::test::clearOrderingTable(core, kOt);
  tomba::render::PacketPool(core).setCursor(kPool);
  for (std::uint32_t offset = 0; offset < 0x400u; offset += 4u) {
    core.mem_w32(0x1F800000u + offset, inputs.word());
  }
  for (std::uint32_t reg = 1; reg < 32u; ++reg) {
    core.r[reg] = inputs.word();
  }
  core.r[29] = kStack;
  core.r[31] = kReturn;
}

// A random rotation near the identity, a translation in front of the camera and the retail projection.
void randomCamera(Core &core, Inputs &inputs) {
  for (std::uint32_t reg = 0; reg < 5u; ++reg) {
    const auto diagonal = static_cast<std::uint16_t>(reg % 2u == 0u ? 0x1000 + inputs.range(-0x400, 0x400) : 0);
    core.gte().writeControl(reg, diagonal | (static_cast<std::uint32_t>(inputs.range(-0x400, 0x400)) << 16));
  }
  core.gte().writeControl(psx::gte::kTranslationX, static_cast<std::uint32_t>(inputs.range(-3000, 3000)));
  core.gte().writeControl(psx::gte::kTranslationY, static_cast<std::uint32_t>(inputs.range(-3000, 3000)));
  core.gte().writeControl(psx::gte::kTranslationZ, static_cast<std::uint32_t>(inputs.range(-500, 40000)));
  for (const std::uint32_t far : {psx::gte::kFarRed, psx::gte::kFarGreen, psx::gte::kFarBlue}) {
    core.gte().writeControl(far, static_cast<std::uint32_t>(inputs.range(0, 0x1000)));
  }
  core.gte().writeControl(psx::gte::kOfx, 160u << 16);
  core.gte().writeControl(psx::gte::kOfy, 120u << 16);
  core.gte().writeControl(psx::gte::kH, 350u);
  core.gte().writeControl(30u, static_cast<std::uint32_t>(inputs.range(0x40, 0x200))); // ZSF4
}

bool testQuads(Core &core, psx::cpu::ImageIdentity image) {
  constexpr std::uint32_t kPlacement = kData;
  constexpr std::uint32_t kIndex = kData + 0x10u;
  constexpr std::uint32_t kAttrs = kData + 0x20u;
  constexpr std::uint32_t kDefs = kData + 0x100u;
  Inputs inputs(tomba::UiSprite::kQuads);
  for (int run = 0; run < kCases; ++run) {
    clearFrame(core, inputs);
    for (std::uint32_t offset = 0; offset < 0x200u; offset += 4u) {
      core.mem_w32(kData + offset, inputs.word());
    }
    const auto extent = [&] {
      return static_cast<std::uint16_t>(inputs.range(0, 2) == 0 ? 0 : inputs.range(-12, 40));
    };
    core.mem_w16(kPlacement + 4u, extent());
    core.mem_w16(kPlacement + 6u, extent());
    core.mem_w16(kIndex, 1u);
    core.mem_w16(kAttrs + 2u, static_cast<std::uint16_t>(inputs.range(0, 1) == 0 ? 0u : inputs.word()));
    core.mem_w8(kAttrs + 1u, static_cast<std::uint8_t>(inputs.range(0, 0x7FF)));
    core.mem_w16(kDefs + 4u, static_cast<std::uint16_t>(inputs.range(1, 5)));
    core.mem_w16(kDefs + 6u, 0x10u);
    core.r[4] = kPlacement;
    core.r[5] = kIndex;
    core.r[6] = kDefs;
    core.r[7] = kAttrs;
    GuestSnapshot native;
    const GuestSnapshot before = capture(core);
    const auto guest = psx::cpu::callOriginal(core, {image, tomba::UiSprite::kQuads}, kBudget);
    const GuestSnapshot authentic = capture(core);
    restore(core, before);
    tomba::UiSprite::quads(&core);
    native = capture(core);
    if (!guest.returned() || authentic.ram != native.ram || authentic.scratch != native.scratch ||
        authentic.r[2] != native.r[2] || authentic.r[29] != native.r[29]) {
      fail("quads", run, authentic, native);
      return false;
    }
  }
  return true;
}

// Records of the packed-mesh format with random bytes; only the last has word 1 not positive.
struct Mesh {
  std::uint32_t list;
  std::uint32_t records;
};
void randomMesh(Core &core, Inputs &inputs, const Mesh &mesh) {
  const auto [list, records] = mesh;
  for (std::uint32_t record = 0; record < records; ++record) {
    const std::uint32_t at = list + record * tomba::render::PackedMesh::kRecordBytes;
    for (std::uint32_t offset = 0; offset < tomba::render::PackedMesh::kRecordBytes; offset += 4u) {
      core.mem_w32(at + offset, inputs.word());
    }
    std::uint32_t word1 = core.mem_r32(at + 4u) & 0x7FFFFFFFu;
    if (record + 1u == records) {
      word1 = inputs.range(0, 3) == 0 ? 0u : word1 | 0x80000000u;
    } else if (word1 == 0u) {
      word1 = 1u;
    }
    core.mem_w32(at + 4u, word1);
    for (const std::uint32_t corner :
         {0x0Fu, 0x13u, 0x17u, 0x1Bu, 0x1Cu, 0x1Du, 0x1Eu, 0x1Fu, 0x20u, 0x21u, 0x22u, 0x23u}) {
      core.mem_w8(at + corner, static_cast<std::uint8_t>(inputs.range(0, 1) == 0 ? inputs.range(0, 8) : inputs.word()));
    }
  }
}

bool renderMatches(Core &core,
                   std::uint32_t object,
                   const GuestSnapshot &afterEmit,
                   psx::present::StateProducerTable &renders) {
  const psx::present::FrameRecord frame = tomba::test::submittedFrame(core, kOt);
  const psx::present::FrameState state = core.frameStates.collect(frame);
  core.frameStates.clear();
  const auto drawn = tomba::test::primitivesOf(frame, tomba::render::PackedMesh::kWriter);
  const auto saved = state.find({tomba::render::PackedMesh::kWriter, object});
  if (!saved) {
    return drawn.empty();
  }
  const auto composed = tomba::test::composedPrimitives(
      renders, {.shown = frame, .from = state, .to = state}, {tomba::render::PackedMesh::kWriter, object});
  const GuestSnapshot afterRender = capture(core);
  return tomba::test::sameFrame(composed, drawn) && afterRender.ram == afterEmit.ram &&
         afterRender.scratch == afterEmit.scratch;
}

bool testPackedMesh(Core &core, psx::cpu::ImageIdentity image, psx::present::StateProducerTable &renders) {
  Inputs inputs(tomba::render::PackedMesh::kWriter);
  for (int run = 0; run < kCases; ++run) {
    clearFrame(core, inputs);
    randomMesh(core, inputs, {.list = kData, .records = static_cast<std::uint32_t>(inputs.range(1, 12))});
    core.mem_w32(tomba::render::PackedMesh::kDepthCue, static_cast<std::uint32_t>(inputs.range(0, 0x1000)));
    core.r[4] = kData;
    core.r[5] = static_cast<std::uint32_t>(inputs.range(0, 3));
    core.r[6] = static_cast<std::uint32_t>(inputs.range(-0x200, 0x200));
    core.r[7] = inputs.word() & 0xFFu;
    randomCamera(core, inputs);
    GuestSnapshot native;
    const GuestSnapshot before = capture(core);
    const auto control = psx::present::readGteControl();
    const auto guest = psx::cpu::callOriginal(core, {image, tomba::render::PackedMesh::kWriter}, kBudget);
    const GuestSnapshot authentic = capture(core);
    restore(core, before);
    psx::present::writeGteControl(control);
    core.frameStates.clear();
    tomba::render::PackedMesh::write(&core);
    native = capture(core);
    if (!guest.returned() || authentic.ram != native.ram || authentic.scratch != native.scratch ||
        authentic.r[29] != native.r[29]) {
      fail("packed mesh", run, authentic, native);
      return false;
    }
    if (!renderMatches(core, kData, native, renders)) {
      lucent::error("authentic-writers", "packed mesh case {}: the t = 1 render differs from the packets written", run);
      ++failed;
      return false;
    }
  }
  return true;
}

// The shadow's own frame and the callee frames below the caller's sp are dead once it returns, and the native
// lays its compose out in FUN_800318A0's frame; everything else matches.
bool testDropShadow(Core &core, psx::cpu::ImageIdentity image) {
  constexpr std::uint32_t kNode = kData + 0x1000u;
  constexpr std::uint32_t kDeadStack = 0x200u;
  Inputs inputs(tomba::render::DropShadow::kDraw);
  for (int run = 0; run < kCases; ++run) {
    clearFrame(core, inputs);
    for (std::uint32_t offset = 0; offset < 0x60u; offset += 4u) {
      core.mem_w32(kNode + offset, inputs.word());
    }
    core.mem_w16(kNode + 0x4Eu, static_cast<std::uint16_t>(inputs.range(0, 0x80)));
    core.mem_w16(kNode + 0x50u, static_cast<std::uint16_t>(inputs.range(0, 0x100)));
    core.mem_w16(kNode + 0x40u, static_cast<std::uint16_t>(inputs.range(0, 0x400)));
    core.mem_w16(kNode + 0x42u, static_cast<std::uint16_t>(inputs.range(0, 0x400)));
    for (const std::uint32_t axis : {0x48u, 0x4Au, 0x4Cu}) {
      core.mem_w16(kNode + axis, static_cast<std::uint16_t>(inputs.range(-4000, 4000)));
    }
    for (std::uint32_t word = 0; word < 5u; ++word) {
      const auto diagonal = static_cast<std::uint16_t>(word % 2u == 0u ? 0x1000 + inputs.range(-0x200, 0x200) : 0);
      core.mem_w32(0x1F8000F8u + word * 4u, diagonal | (static_cast<std::uint32_t>(inputs.range(-0x200, 0x200)) << 16));
    }
    core.mem_w32(0x1F80010Cu, static_cast<std::uint32_t>(inputs.range(-2000, 2000)));
    core.mem_w32(0x1F800110u, static_cast<std::uint32_t>(inputs.range(-2000, 2000)));
    core.mem_w32(0x1F800114u, static_cast<std::uint32_t>(inputs.range(500, 20000)));
    randomCamera(core, inputs);
    core.r[4] = kNode;
    const GuestSnapshot before = capture(core);
    const auto control = psx::present::readGteControl();
    const auto guest = psx::cpu::callOriginal(core, {image, tomba::render::DropShadow::kDraw}, kBudget);
    GuestSnapshot authentic = capture(core);
    const auto guestControl = psx::present::readGteControl();
    restore(core, before);
    psx::present::writeGteControl(control);
    tomba::render::DropShadow::draw(core);
    GuestSnapshot native = capture(core);
    const std::size_t dead = (kStack & 0x1FFFFFu) - kDeadStack;
    std::fill_n(authentic.ram.begin() + static_cast<std::ptrdiff_t>(dead), kDeadStack, std::uint8_t{0});
    std::fill_n(native.ram.begin() + static_cast<std::ptrdiff_t>(dead), kDeadStack, std::uint8_t{0});
    if (!guest.returned() || authentic.ram != native.ram || authentic.scratch != native.scratch ||
        authentic.r[29] != native.r[29] || authentic.r[16] != native.r[16] || authentic.r[17] != native.r[17] ||
        authentic.r[18] != native.r[18] || authentic.r[19] != native.r[19] ||
        guestControl != psx::present::readGteControl()) {
      fail("drop shadow", run, authentic, native);
      return false;
    }
  }
  return true;
}

// One emitter node over particles of the format FUN_8003B220 reads, its bucket bias zero.
void writeBillboard(Core &core, std::uint32_t node) {
  constexpr std::uint32_t kTable = kData + 0x800u;
  constexpr std::uint32_t kList = kData + 0x900u;
  for (std::uint32_t offset = 0; offset < 0x60u; offset += 4u) {
    core.mem_w32(node + offset, 0u);
  }
  core.mem_w32(node + 56u, kTable);
  core.mem_w32(node + 60u, kList);
  core.mem_w16(kTable, 0u);
  core.mem_w16(kList, 2u);
  core.mem_w16(kList + 2u, 0x10u);
  for (std::uint32_t particle = 0; particle < 2u; ++particle) {
    const std::uint32_t at = kList + 0x10u + particle * 16u;
    core.mem_w32(at, 0x00100010u + particle * 0x20u);
    core.mem_w32(at + 4u, 0x00200010u);
    core.mem_w32(at + 8u, 0x7C001010u);
    core.mem_w32(at + 12u, 0x00082020u);
  }
}

bool testBillboard(Core &core, tomba::test::ComposeHarness &harness) {
  constexpr std::uint32_t kNode = kData + 0x2000u;
  const auto draw = [&](std::uint32_t x) {
    return harness.frame([&] {
      for (std::uint32_t reg = 0; reg < 5u; ++reg) {
        core.gte().writeControl(reg, reg % 2u == 0u ? 0x1000u : 0u);
      }
      core.gte().writeControl(psx::gte::kTranslationX, x);
      core.gte().writeControl(psx::gte::kTranslationY, 0u);
      core.gte().writeControl(psx::gte::kTranslationZ, 1500u);
      core.gte().writeControl(psx::gte::kOfx, 160u << 16);
      core.gte().writeControl(psx::gte::kOfy, 120u << 16);
      core.gte().writeControl(psx::gte::kH, 350u);
      core.gte().writeControl(29u, 0x155u);
      core.gte().writeControl(30u, 0x100u);
      writeBillboard(core, kNode);
      core.r[4] = kNode;
      core.r[5] = 0u;
      core.r[29] = kStack;
      core.r[31] = kReturn;
      tomba::rend(&core)->billboardEmit();
    });
  };
  const auto from = draw(0u);
  const auto to = draw(200u);
  const auto midway = draw(100u);
  lucent::info("authentic-writers", "billboard: {} primitives drawn", harness.primitives(to.shown).size());
  return harness.composesAsDrawn(from, to) && harness.composesBetween(from, to, midway.shown);
}

int runTests(int argc, char **argv) {
  if (argc != 3) {
    lucent::error("authentic-writers", "expected MAIN.EXE path and manifest digest");
    return 2;
  }
  const auto exe =
      tomba::test::readAuthenticatedImage({.path = argv[1], .sha256 = argv[2], .maxBytes = psx::cpu::kPsxExeMaxBytes});
  if (!exe) {
    return 2;
  }
  tomba::Runtime runtime;
  auto game = std::make_unique<Game>(runtime, psx::config::Config{});
  Core &core = game->core;
  game->mods.aspect = ASPECT_4_3;
  auto loaded = psx::cpu::loadPsxExeImage(core, *exe, "MAIN.EXE");
  if (!loaded.identity) {
    lucent::error("authentic-writers", "MAIN.EXE did not map through the shipping loader");
    return 1;
  }
  core.gte().bind();
  tomba::test::ComposeHarness harness(core, kOt, kPool);
  tomba::render::PackedMesh::registerStateRenders(harness.renders(), core);
  tomba::render::BillboardState::registerStateRenders(harness.renders(), core);

  const psx::cpu::ImageIdentity image = *loaded.identity;
  const bool passed = testQuads(core, image) && testPackedMesh(core, image, harness.renders()) &&
                      testDropShadow(core, image) && testBillboard(core, harness);
  if (!passed || failed != 0) {
    lucent::error("authentic-writers", "FAIL");
    return 1;
  }
  lucent::info("authentic-writers",
               "PASS: {} cases each of quads, packed mesh and drop shadow byte-identical to the guest; packed mesh "
               "t = 1 renders match; billboard composes at t = 1 and 0.5",
               kCases);
  return 0;
}

} // namespace

int main(int argc, char **argv) {
  try {
    return runTests(argc, argv);
  } catch (const std::exception &error) {
    lucent::error("test", "FAIL: {}", error.what());
    return 1;
  }
}
