// FxSpritePublish: the sprite family's scene camera load, anchor projection and OT key gate.
#include "fx_sprite_publish.h"
#include "game.h"
#include "gte_registers.h"
#include "hw_bind.h"
#include "ordering_table.h"
#include "stub_runtime.h"

#include <cstdint>
#include <lucent/log.h>
#include <memory>

namespace {

namespace gte = tomba2::gte;
using tomba2::render::OrderingTable;

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

bool project(Core &core, std::uint32_t depth, std::int32_t otBias, std::uint32_t dqa = 6u) {
  parkCamera(core, depth);
  clearSlots(core);
  FxSpritePublish publish{&core};
  publish.loadSceneCamera(dqa);
  return publish.projectAnchor(0u, 0u, otBias);
}

void testCameraLoad(Core &core) {
  parkCamera(core, 777u);
  gte_write_ctrl(gte::kDqb, 99u);
  const FxSpritePublish publish{&core};
  publish.loadSceneCamera(4u);
  bool same = true;
  for (std::uint32_t i = 0; i < 8u; i++) {
    same = same && gte_read_ctrl(gte::kRotation + i) == core.mem_r32(kSceneCamera + i * 4u);
  }
  check(same, "the scratchpad camera lands in CR0-7");
  check(gte_read_ctrl(gte::kDqa) == 4u && gte_read_ctrl(gte::kDqb) == 0u, "DQA is the emitter's, DQB is zero");
}

void testVisible(Core &core) {
  check(project(core, 1024u, 10), "an anchor in front is published");
  check(core.mem_r32(fxpublish::kOtKey) == static_cast<std::uint32_t>(OrderingTable::compressDepth((1024 >> 2) + 10)),
        "the key is SZ3 >> 2 plus the bias, compressed");
  check(core.mem_r32(fxpublish::kScreenXY) == ((kOffsetY << 16) | kOffsetX), "the screen anchor is SXY2");
  const std::uint32_t mac0 = gte_read_data(gte::kMac0);
  check(core.mem_r32(fxpublish::kScaleX) == mac0, "the scale is MAC0");
  check(project(core, 1024u, 10, 4u) && core.mem_r32(fxpublish::kScaleX) * 3u == mac0 * 2u && mac0 != 0u,
        "MAC0 is the divide times the emitter's DQA");
}

void testNearClamp(Core &core) {
  check(project(core, 1024u, -300), "a key nearer than the nearest bucket is still published");
  check(core.mem_r32(fxpublish::kOtKey) == static_cast<std::uint32_t>(OrderingTable::kNearestBucket),
        "the key clamps to the nearest bucket");
}

void testFlagCull(Core &core) {
  // SZ3 below H / 2 overflows the divide, which raises the FLAG error summary.
  check(!project(core, 8u, 0), "a divide overflow culls");
  check(static_cast<std::int32_t>(core.mem_r32(fxpublish::kOtKey)) < 0, "the key slot keeps the FLAG word");
  check(core.mem_r32(fxpublish::kOtKey) == gte_read_ctrl(gte::kFlag), "the FLAG word is stored as read");
  check(core.mem_r32(fxpublish::kScreenXY) == kUntouched && core.mem_r32(fxpublish::kScaleX) == kUntouched,
        "a culled anchor publishes no position or scale");
}

void testDepthRangeCull(Core &core) {
  check(!project(core, 40000u, 0), "a key past the far bucket culls");
  check(core.mem_r32(fxpublish::kOtKey) == static_cast<std::uint32_t>(OrderingTable::kNoBucket),
        "the trail ends in the no-bucket key");
  check(core.mem_r32(fxpublish::kScreenXY) == kUntouched && core.mem_r32(fxpublish::kScaleX) == kUntouched,
        "an out-of-range anchor publishes no position or scale");
}

} // namespace

int main() {
  tomba::test::StubRuntime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  gte_bind(&core);
  gte_write_ctrl(gte::kOfx, kOffsetX << 16);
  gte_write_ctrl(gte::kOfy, kOffsetY << 16);
  gte_write_ctrl(gte::kH, kScreenH);
  testCameraLoad(core);
  testVisible(core);
  testNearClamp(core);
  testFlagCull(core);
  testDepthRangeCull(core);
  lucent::info("fx-sprite-publish-test", "checked={} failed={}", checked, failed);
  return failed == 0 ? 0 : 1;
}
