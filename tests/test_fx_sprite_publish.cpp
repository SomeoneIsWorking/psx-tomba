// FxSpritePublish: the sprite family's scene camera load, anchor projection and OT key gate.
#include "fx_sprite_publish.h"
#include "game.h"
#include "gte_registers.h"
#include "guest_ordering_table.h"
#include "stub_game_runtime.h"

#include <cstdint>
#include <lucent/log.h>
#include <memory>

namespace {

using tomba::render::OrderingTable;

constexpr std::uint32_t kSceneCamera = 0x1F8000F8u;
constexpr std::uint32_t kUnity = 0x1000u;
constexpr std::uint32_t kUntouched = 0xA5A5A5A5u;
constexpr std::uint32_t kOffsetX = 160u;
constexpr std::uint32_t kOffsetY = 120u;
constexpr std::uint32_t kScreenH = 256u;

int failed = 0;
int checked = 0;

void check(bool condition, const char *name) {
  ++checked;
  if (!condition) {
    ++failed;
    lucent::error("fx-sprite-publish-test", "failed: {}", name);
  }
}

// Identity rotation, translation (0, 0, depth).
void parkCamera(Core &core, std::uint32_t depth) {
  const std::uint32_t words[8] = {kUnity, 0u, kUnity, 0u, kUnity, 0u, 0u, depth};
  for (std::uint32_t i = 0; i < 8u; i++) {
    core.mem_w32(kSceneCamera + i * 4u, words[i]);
  }
}

void clearSlots(Core &core) {
  for (const std::uint32_t slot : {fxpublish::kOtKey, fxpublish::kScreenXY, fxpublish::kScaleX}) {
    core.mem_w32(slot, kUntouched);
  }
}

struct ProjectArgs {
  std::uint32_t depth;
  std::int32_t otBias;
  std::uint32_t dqa = 6u;
};
bool project(Core &core, const ProjectArgs &args) {
  auto [depth, otBias, dqa] = args;
  parkCamera(core, depth);
  clearSlots(core);
  tomba::FxSpritePublish publish{&core};
  publish.loadSceneCamera(dqa);
  return publish.projectAnchor({.worldXY = 0u, .worldZ = 0u, .otBias = otBias});
}

void testCameraLoad(Core &core) {
  parkCamera(core, 777u);
  core.gte().writeControl(psx::gte::kDqb, 99u);
  const tomba::FxSpritePublish publish{&core};
  publish.loadSceneCamera(4u);
  bool same = true;
  for (std::uint32_t i = 0; i < 8u; i++) {
    same = same && core.gte().readControl(psx::gte::kRotation + i) == core.mem_r32(kSceneCamera + i * 4u);
  }
  check(same, "the scratchpad camera lands in CR0-7");
  check(core.gte().readControl(psx::gte::kDqa) == 4u && core.gte().readControl(psx::gte::kDqb) == 0u,
        "DQA is the emitter's, DQB is zero");
}

void testVisible(Core &core) {
  check(project(core, {.depth = 1024u, .otBias = 10}), "an anchor in front is published");
  check(core.mem_r32(fxpublish::kOtKey) == static_cast<std::uint32_t>(OrderingTable::compressDepth((1024 >> 2) + 10)),
        "the key is SZ3 >> 2 plus the bias, compressed");
  check(core.mem_r32(fxpublish::kScreenXY) == ((kOffsetY << 16) | kOffsetX), "the screen anchor is SXY2");
  const std::uint32_t mac0 = core.gte().readData(psx::gte::kMac0);
  check(core.mem_r32(fxpublish::kScaleX) == mac0, "the scale is MAC0");
  check(project(core, {.depth = 1024u, .otBias = 10, .dqa = 4u}) &&
            core.mem_r32(fxpublish::kScaleX) * 3u == mac0 * 2u && mac0 != 0u,
        "MAC0 is the divide times the emitter's DQA");
}

void testNearClamp(Core &core) {
  check(project(core, {.depth = 1024u, .otBias = -300}), "a key nearer than the nearest bucket is still published");
  check(core.mem_r32(fxpublish::kOtKey) == static_cast<std::uint32_t>(OrderingTable::kNearestBucket),
        "the key clamps to the nearest bucket");
}

void testFlagCull(Core &core) {
  // SZ3 below H / 2 overflows the divide, which raises the FLAG error summary.
  check(!project(core, {.depth = 8u, .otBias = 0}), "a divide overflow culls");
  check(static_cast<std::int32_t>(core.mem_r32(fxpublish::kOtKey)) < 0, "the key slot keeps the FLAG word");
  check(core.mem_r32(fxpublish::kOtKey) == core.gte().readControl(psx::gte::kFlag), "the FLAG word is stored as read");
  check(core.mem_r32(fxpublish::kScreenXY) == kUntouched && core.mem_r32(fxpublish::kScaleX) == kUntouched,
        "a culled anchor publishes no position or scale");
}

void testDepthRangeCull(Core &core) {
  check(!project(core, {.depth = 40000u, .otBias = 0}), "a key past the far bucket culls");
  check(core.mem_r32(fxpublish::kOtKey) == static_cast<std::uint32_t>(OrderingTable::kNoBucket),
        "the trail ends in the no-bucket key");
  check(core.mem_r32(fxpublish::kScreenXY) == kUntouched && core.mem_r32(fxpublish::kScaleX) == kUntouched,
        "an out-of-range anchor publishes no position or scale");
}

} // namespace

int main() {
  psx::test::StubGameRuntime runtime;
  auto game = std::make_unique<Game>(runtime, psx::config::Config{});
  Core &core = game->core;
  core.gte().bind();
  core.gte().writeControl(psx::gte::kOfx, kOffsetX << 16);
  core.gte().writeControl(psx::gte::kOfy, kOffsetY << 16);
  core.gte().writeControl(psx::gte::kH, kScreenH);
  testCameraLoad(core);
  testVisible(core);
  testNearClamp(core);
  testFlagCull(core);
  testDepthRangeCull(core);
  lucent::info("fx-sprite-publish-test", "checked={} failed={}", checked, failed);
  return failed == 0 ? 0 : 1;
}
