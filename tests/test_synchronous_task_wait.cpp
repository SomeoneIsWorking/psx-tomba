// FUN_80044BD4's completion tail: flag != 1 stamps the task with the guest RNG, flag 1 leaves it.
#include "core/entry/runtime.h"
#include "core/sched/scheduler.h"
#include "core/sched/task_slot_layout.h"
#include "game.h"
#include "math/rng.h"
#include "testutil.h"

#include <memory>

namespace {

using tomba::sched::kSyncWaitStamp;
using tomba::sched::kTaskTableBase;

constexpr std::uint32_t kWaitCounter = 0x1F800198u;
// Seed 1: 1 * 0x41C64E6D + 0x3039 = 0x41C67EA6, so the stamp is 0x41C6.
constexpr std::uint16_t kStampFromSeedOne = 0x41C6u;

std::unique_ptr<Game> makeGame(tomba::Runtime &runtime) {
  auto game = std::make_unique<Game>(runtime, psx::config::Config{});
  game->core.mem_w32(tomba::Rng::SEED_ADDR, 1u);
  game->core.mem_w16(kWaitCounter, 0x4567u);
  game->core.mem_w16(kTaskTableBase + kSyncWaitStamp, 0xBEEFu);
  return game;
}

void test_flag_two_stamps_from_the_guest_rng() {
  tomba::Runtime runtime;
  auto game = makeGame(runtime);
  const auto result = tomba::sched::taskScheduler(game->core).completeSyncWait(kTaskTableBase, 2);
  CHECK(result.stamped);
  CHECK_EQ(result.stamp, kStampFromSeedOne);
  CHECK_EQ(game->core.mem_r16(kTaskTableBase + kSyncWaitStamp), kStampFromSeedOne);
  CHECK_EQ(game->core.mem_r32(tomba::Rng::SEED_ADDR), 0x41C67EA6u);
  CHECK_EQ(game->core.mem_r16(kWaitCounter), 0x4567u);
}

void test_flag_one_leaves_task_and_rng() {
  tomba::Runtime runtime;
  auto game = makeGame(runtime);
  const auto result = tomba::sched::taskScheduler(game->core).completeSyncWait(kTaskTableBase, 1);
  CHECK(!result.stamped);
  CHECK_EQ(game->core.mem_r16(kTaskTableBase + kSyncWaitStamp), 0xBEEFu);
  CHECK_EQ(game->core.mem_r32(tomba::Rng::SEED_ADDR), 1u);
  CHECK_EQ(game->core.mem_r16(kWaitCounter), 0x4567u);
}

void test_flag_three_uses_the_same_completion() {
  tomba::Runtime runtime;
  auto game = makeGame(runtime);
  const auto result = tomba::sched::taskScheduler(game->core).completeSyncWait(kTaskTableBase, 3);
  CHECK(result.stamped);
  CHECK_EQ(result.stamp, kStampFromSeedOne);
  CHECK_EQ(game->core.mem_r16(kWaitCounter), 0x4567u);
}

} // namespace

int main() {
  RUN(flag_two_stamps_from_the_guest_rng);
  RUN(flag_one_leaves_task_and_rng);
  RUN(flag_three_uses_the_same_completion);
  return pt_summary();
}
