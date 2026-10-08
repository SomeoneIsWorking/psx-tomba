// tests/test_cull_far_limits.cpp — Cull keeps the guest's own far limits (FUN_8007712C, FUN_8002B278).
//
// A far limit wider than the guest's keeps objects retail culls; their packets overran the 80 KB
// packet pool at the village vista and corrupted the record-pool cursor at 0x800E7E74.
#include "core.h"
#include "game.h"
#include "render/cull.h"

#include <cstdint>
#include <cstdio>
#include <memory>

namespace {

inline constexpr std::uint32_t kNode = 0x80100000u;
inline constexpr std::uint32_t kCameraX = 0x1F8000D2u;
inline constexpr std::uint32_t kCameraY = 0x1F8000D6u;
inline constexpr std::uint32_t kCameraZ = 0x1F8000DAu;
inline constexpr std::uint32_t kForwardX = 0x1F8000E8u;
inline constexpr std::uint32_t kForwardY = 0x1F8000EAu;
inline constexpr std::uint32_t kForwardZ = 0x1F8000ECu;
inline constexpr std::uint32_t kQueueGate = 0x1F800080u;
inline constexpr std::uint32_t kCullState = 0x1F800084u;
inline constexpr std::uint32_t kAreaMode = 0x800BF870u;
inline constexpr std::uint32_t kVisible = 1u;
inline constexpr std::uint32_t kPosition = 0x2Cu;

int failures = 0;

void check(bool condition, const char *detail) {
  if (!condition) {
    std::printf("FAIL: %s\n", detail);
    ++failures;
  }
}

void lookDownZ(Core &core) {
  core.mem_w16(kCameraX, 0);
  core.mem_w16(kCameraY, 0);
  core.mem_w16(kCameraZ, 0);
  core.mem_w16(kForwardX, 0);
  core.mem_w16(kForwardY, 0);
  core.mem_w16(kForwardZ, 0x1000);
}

bool coneKeeps(Core &core, Cull &cull, int16_t z) {
  core.mem_w8(kNode + kVisible, 0);
  core.mem_w16(kNode + kPosition + 0, 0);
  core.mem_w16(kNode + kPosition + 2, 0);
  core.mem_w16(kNode + kPosition + 4, static_cast<std::uint16_t>(z));
  core.r[4] = kNode;
  cull.coneCull2b278();
  return core.r[2] == 1u && core.mem_r8(kNode + kVisible) == 1u;
}

bool baseKeeps(Core &core, Cull &cull, std::uint32_t state, int16_t dz) {
  core.mem_w32(kCullState, state);
  core.mem_w8(kNode + kVisible, 0);
  core.r[4] = kNode;
  core.r[5] = 0;
  core.r[6] = 0;
  core.r[7] = static_cast<std::uint32_t>(static_cast<std::int32_t>(dz));
  cull.performBaseCull();
  return core.r[2] == 1u && core.mem_r8(kNode + kVisible) == 1u;
}

} // namespace

int main() {
  auto game = std::make_unique<Game>();
  auto core = std::make_unique<Core>();
  core->game = game.get();
  Cull cull;
  cull.core = core.get();
  lookDownZ(*core);
  core->mem_w32(kQueueGate, 1);
  core->mem_w8(kAreaMode, 0);

  // FUN_8002B278: keep iff 0x200 <= dist < 0x1C01 and fwd.d >= dist * 0xD60.
  check(coneKeeps(*core, cull, 7168), "cone cull dropped an object inside the guest far limit");
  check(!coneKeeps(*core, cull, 7169), "cone cull kept an object at the guest far limit 0x1C01");
  check(!coneKeeps(*core, cull, 28000), "cone cull kept an object four times past the guest far limit");
  check(!coneKeeps(*core, cull, 511), "cone cull kept an object inside the guest near limit");

  // FUN_8007712C state 1 {512, 7169, 856}, state 2 {768, 4097, 880}, state 3 {512, 4097, 848}.
  check(baseKeeps(*core, cull, 1, 7168), "base cull state 1 dropped an object inside its far limit");
  check(!baseKeeps(*core, cull, 1, 7169), "base cull state 1 kept an object at its far limit");
  check(baseKeeps(*core, cull, 2, 4096), "base cull state 2 dropped an object inside its far limit");
  check(!baseKeeps(*core, cull, 2, 4097), "base cull state 2 kept an object at its far limit");
  check(!baseKeeps(*core, cull, 3, 16000), "base cull state 3 kept an object four times past its far limit");

  std::printf("Tomba cull far limits: %s\n", failures == 0 ? "PASS" : "FAIL");
  return failures == 0 ? 0 : 1;
}
