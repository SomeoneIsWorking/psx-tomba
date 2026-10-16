// The `warp` command arms only in the running field and inside the debug area select's bounds, and applies as the
// door at 0x8002CFFC does: destination words, stopBgm(1) on an area change, then area machine 7 state 0.
#include "context.h"
#include "core.h"
#include "dev_warp.h"
#include "game.h"
#include "stub_game_runtime.h"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>

namespace {

using tomba1::DevWarp;

inline constexpr std::uint32_t kRecord = 0x801FD800u;
inline constexpr std::uint32_t kStopBgmSeen = 0x80001000u;
inline constexpr std::uint32_t kStack = 0x801E0000u;

int failures = 0;

void check(bool condition, const char *detail) {
  if (!condition) {
    std::printf("FAIL: %s\n", detail);
    ++failures;
  }
}

bool starts(const std::string &text, const char *prefix) {
  return text.rfind(prefix, 0) == 0;
}

void setMachine(Core &core, std::uint16_t machine, std::uint16_t state) {
  core.mem_w16(kRecord + DevWarp::kMachineOffset, machine);
  core.mem_w16(kRecord + DevWarp::kStateOffset, state);
}

// stopBgm stand-in: records a0, returns.
void plantStopBgm(Core &core) {
  core.mem_w32(DevWarp::kStopBgm + 0x0u, 0x3C088000u); // lui  t0,0x8000
  core.mem_w32(DevWarp::kStopBgm + 0x4u, 0xAD041000u); // sw   a0,0x1000(t0)
  core.mem_w32(DevWarp::kStopBgm + 0x8u, 0x03E00008u); // jr   ra
  core.mem_w32(DevWarp::kStopBgm + 0xCu, 0x00000000u);
}

} // namespace

int main() {
  psx::test::StubGameRuntime runtime;
  auto game = std::make_unique<Game>(runtime, psx::config::Config{});
  Core &core = game->core;
  core.r[29] = kStack;
  plantStopBgm(core);
  core.imageCatalog().activate(
      "tomba1-dev-warp", {DevWarp::kStopBgm & 0x1FFFFFFFu, (DevWarp::kStopBgm & 0x1FFFFFFFu) + 0x10u}, 0x57415250ull);
  core.mem_w16(DevWarp::kLastArea, 3u);
  core.mem_w16(DevWarp::kSectionCounts + 1u * 2u, 2u);
  core.mem_w16(DevWarp::kSectionCounts + 2u * 2u, 1u);
  core.mem_w16(DevWarp::kCurrentArea, 0u);
  core.mem_w32(DevWarp::kTaskRecordPointer, kRecord);

  DevWarp warp;
  setMachine(core, 0u, 3u);
  check(starts(warp.arm(core, "warp 1"), "refused"), "warp refused outside the running field");
  setMachine(core, DevWarp::kFieldMachine, 0u);
  check(starts(warp.arm(core, "warp 1"), "refused"), "warp refused while the field initialises");
  setMachine(core, DevWarp::kFieldMachine, DevWarp::kFieldRunning);
  check(starts(warp.arm(core, "warp"), "usage"), "warp needs an area");
  check(starts(warp.arm(core, "warp x"), "usage"), "warp needs a decimal area");
  check(starts(warp.arm(core, "warp 4"), "refused"), "warp refuses an area past the last");
  check(starts(warp.arm(core, "warp 2 1"), "refused"), "warp refuses a section the area lacks");
  check(starts(warp.arm(core, "warp 1 1 256"), "refused"), "warp refuses an entry past 0xFF");

  warp.applyArmed(core, 10u);
  check(core.mem_r16(kRecord + DevWarp::kMachineOffset) == DevWarp::kFieldMachine, "a refused warp writes nothing");

  check(starts(warp.arm(core, "warp 1 1 3"), "ok"), "warp arms in the running field");
  warp.applyArmed(core, 11u);
  check(core.mem_r16(DevWarp::kDestinationArea) == 1u, "destination area written");
  check(core.mem_r16(DevWarp::kDestinationSection) == 1u, "destination section written");
  check(core.mem_r16(DevWarp::kDestinationEntry) == 3u, "destination entry written");
  check(core.mem_r32(kStopBgmSeen) == 1u, "stopBgm(1) runs on an area change");
  check(core.mem_r16(kRecord + DevWarp::kMachineOffset) == DevWarp::kTransitionMachine, "machine is the transition");
  check(core.mem_r16(kRecord + DevWarp::kStateOffset) == 0u, "the transition starts at state 0");

  core.mem_w32(kStopBgmSeen, 0u);
  core.mem_w16(DevWarp::kCurrentArea, 2u);
  setMachine(core, DevWarp::kFieldMachine, DevWarp::kFieldRunning);
  check(starts(warp.arm(core, "warp 2"), "ok"), "warp arms within the current area");
  setMachine(core, 8u, 0u);
  warp.applyArmed(core, 12u);
  check(core.mem_r16(kRecord + DevWarp::kMachineOffset) == 8u, "a warp the field left before its boundary is dropped");
  setMachine(core, DevWarp::kFieldMachine, DevWarp::kFieldRunning);
  check(starts(warp.arm(core, "warp 2"), "ok"), "warp re-arms");
  warp.applyArmed(core, 13u);
  check(core.mem_r32(kStopBgmSeen) == 0u, "stopBgm is not called within the same area");
  check(core.mem_r16(kRecord + DevWarp::kMachineOffset) == DevWarp::kTransitionMachine, "same-area warp transitions");

  if (failures != 0) {
    std::printf("%d failure(s)\n", failures);
    return 1;
  }
  std::printf("dev warp: all checks passed\n");
  return 0;
}
