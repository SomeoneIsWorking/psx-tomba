// The developer commands (warp, items, flag): each is parsed and armed between frames, and written only when the
// frame driver applies it at the next frame boundary.
#include "core.h"
#include "core/engine/task_sm.h"
#include "debug/dev_flags.h"
#include "debug/dev_items.h"
#include "debug/dev_warp.h"
#include "game.h"
#include "items/inventory_layout.h"
#include "scene/scene_flags.h"
#include "stub_runtime.h"

#include <cstdint>
#include <lucent/log.h>
#include <memory>
#include <string>

namespace {

constexpr std::uint32_t kStagePointer = 0x801FE00Cu;
constexpr std::uint32_t kGameStage = 0x8010637Cu;
constexpr std::uint32_t kTaskRecord = 0x801FE000u;
constexpr std::uint32_t kPending = 0x800BF839u;
constexpr std::uint32_t kDestination = 0x800BF83Au;
constexpr std::uint32_t kLoadMode = 0x800BF89Cu;
constexpr std::uint32_t kTransitionType = 0x1F800236u;
constexpr std::uint32_t kInventoryCounts = inventory_layout::kCounts;
constexpr std::uint32_t kQuestPass = inventory_layout::kQuestPassCounter;

int failed = 0;
int checked = 0;

void check(bool condition, const char *name) {
  ++checked;
  if (!condition) {
    ++failed;
    lucent::error("dev-control-test", "failed: {}", name);
  }
}

bool starts(const std::string &text, const char *prefix) {
  return text.rfind(prefix, 0) == 0;
}

// The field running: top 2, sub-mode 1, area machine `machine`, sub-state 1, ordinary play.
void fieldRunning(Core &core, std::uint16_t machine) {
  core.mem_w32(kStagePointer, kGameStage);
  core.mem_w32(TaskSm::kTaskRecordPointer, kTaskRecord);
  TaskSm sm(&core);
  sm.setTop(2);
  sm.setSubMode(1);
  sm.setStage4c(machine);
  sm.setS4e(1);
  core.mem_w8(kLoadMode, 4);
  core.mem_w8(kPending, 0);
}

void testWarp(Core &core) {
  tomba::DevWarp control;
  core.mem_w32(kStagePointer, 0);
  check(starts(control.arm(core, "warp 3 2"), "refused"), "warp refused outside the GAME stage");

  fieldRunning(core, 2);
  check(starts(control.arm(core, "warp 22"), "refused"), "warp refuses an area past the table");
  check(starts(control.arm(core, "warp 3 64"), "refused"), "warp refuses an entry past 0x3f");
  check(starts(control.arm(core, "warp x"), "usage"), "warp prints usage for junk");

  check(starts(control.arm(core, "warp 3 2"), "ok"), "warp arms in the running field");
  check(core.mem_r8(kPending) == 0, "arming writes nothing");
  control.applyArmed(core, 10);
  check(core.mem_r16(kDestination) == ((3u << 8) | 2u), "field run: destination holds area and entry");
  check(core.mem_r8(kPending) == 1, "field run: the door request is raised");
  core.mem_w8(kPending, 0);
  control.applyArmed(core, 12);
  check(core.mem_r8(kPending) == 0, "an applied request is not applied twice");

  fieldRunning(core, 5);
  control.arm(core, "warp 8");
  control.applyArmed(core, 13);
  check(core.mem_r16(kDestination) == (8u << 8), "handler: destination holds the default entry 0");
  check(core.mem_r8(kPending) == 3, "handler areas take the exit request");

  fieldRunning(core, 2);
  control.arm(core, "warp 4 1");
  core.mem_w32(kStagePointer, 0);
  control.applyArmed(core, 14);
  check(core.mem_r8(kPending) == 0, "a request armed before the field ended is dropped");

  // The scripted opening is left the way the Start skip leaves it.
  core.mem_w32(kStagePointer, kGameStage);
  TaskSm sm(&core);
  sm.setSubMode(0);
  sm.setStage4c(0);
  sm.setS4e(0);
  core.mem_w8(kLoadMode, 2);
  check(starts(control.arm(core, "warp 2 3"), "ok"), "warp arms during the scripted opening");
  control.applyArmed(core, 15);
  const TaskSm after(&core);
  check(core.mem_r8(kLoadMode) == 4, "opening: ordinary-play load mode");
  check(core.mem_r8(kTransitionType) == 1, "opening: full transition type");
  check(after.subMode() == 1 && after.stage4c() == 2 && after.s4e() == 6, "opening: field run in area change");
  check(core.mem_r16(kDestination) == ((2u << 8) | 3u), "opening: destination holds area and entry");
}

void testFlags(Core &core) {
  tomba::DevFlags control;
  const std::uint32_t slot = scene_flags::flagAddr(40);
  core.mem_w8(slot, 7);
  check(control.arm(core, "flag get 40") == "flag 40 = 7", "flag get reads the table byte");
  check(starts(control.arm(core, "flag set 40 200"), "ok"), "flag set arms");
  check(core.mem_r8(slot) == 7, "arming writes nothing");
  control.applyArmed(core, 20);
  check(core.mem_r8(slot) == 200, "flag set writes at the frame boundary");
  check(starts(control.arm(core, "flag set 256 1"), "refused"), "flag index past 255 is refused");
  check(starts(control.arm(core, "flag set 1 256"), "refused"), "flag value past 255 is refused");
  check(starts(control.arm(core, "flag bogus"), "usage"), "flag prints usage for junk");
}

int gives = 0;
std::uint32_t givenItem = 0;
std::uint32_t givenAmount = 0;

void give(Core &, std::uint32_t item, std::uint32_t amount) {
  ++gives;
  givenItem = item;
  givenAmount = amount;
}

void testItems(Core &core) {
  tomba::DevItems control;
  core.mem_w32(kStagePointer, 0);
  check(starts(control.arm(core, "items all"), "refused"), "items refused outside the GAME stage");

  core.mem_w32(kStagePointer, kGameStage);
  check(starts(control.arm(core, "items 300"), "refused"), "item id past 255 is refused");
  check(starts(control.arm(core, "items 5 100"), "refused"), "amount past 99 is refused");
  check(starts(control.arm(core, "items all"), "ok"), "items all arms");
  check(core.mem_r8(kQuestPass) == 0 && gives == 0, "arming grants nothing");
  control.applyArmed(core, &give, 30);
  check(core.mem_r8(kQuestPass) == 0xff, "grant all raises the quest-pass counter");
  bool granted = false;
  for (std::uint32_t id = 0; id < 256; ++id) {
    granted = granted || core.mem_r8(kInventoryCounts + id) != 0;
  }
  check(granted, "grant all fills the inventory counts");
  check(starts(control.arm(core, "items 5 2"), "ok"), "items by id arms");
  control.applyArmed(core, &give, 31);
  check(gives == 1 && givenItem == 5 && givenAmount == 2, "items by id gives through the game's own give");
}

} // namespace

int main() {
  tomba::test::StubRuntime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  testWarp(core);
  testFlags(core);
  testItems(core);
  if (failed != 0) {
    lucent::error("dev-control-test", "{} of {} checks failed", failed, checked);
    return 1;
  }
  lucent::info("dev-control-test", "{} checks passed", checked);
  return 0;
}
