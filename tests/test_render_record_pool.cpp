// RenderRecordPool: FUN_8007AAE8's stack pop, the registers its guest callers see, and the incarnation a
// producer key names, so a record freed by one object and popped for the next never pairs the two.
#include "game.h"
#include "render_record_pool.h"
#include "stub_runtime.h"

#include <cstdint>
#include <lucent/log.h>
#include <memory>

namespace {

using tomba2::world::incarnationObject;
using tomba2::world::RenderRecordPool;

constexpr std::uint32_t kStack = 0x80180000u;
constexpr std::uint32_t kRecordA = 0x800F407Cu;
constexpr std::uint32_t kRecordB = 0x800F40C8u;
constexpr std::uint32_t kProducer = 0x8003CDD8u;
constexpr std::uint32_t kPacket = 0x801A0000u;

int failed = 0;
int checked = 0;

void check(bool condition, const char *name) {
  ++checked;
  if (!condition) {
    ++failed;
    lucent::error("render-record-pool-test", "failed: {}", name);
  }
}

// Two free records, A on top.
void seed(Core &core) {
  core.mem_w32(kStack, kRecordA);
  core.mem_w32(kStack + 4u, kRecordB);
  core.mem_w32(RenderRecordPool::kFreeCursor, kStack);
  core.mem_w16(RenderRecordPool::kFreeCount, 2u);
}

// What despawn does: push the record back on the stack.
void release(Core &core, std::uint32_t record) {
  const std::uint32_t cursor = core.mem_r32(RenderRecordPool::kFreeCursor) - 4u;
  core.mem_w16(RenderRecordPool::kFreeCount,
               static_cast<std::uint16_t>(core.mem_r16(RenderRecordPool::kFreeCount) + 1u));
  core.mem_w32(RenderRecordPool::kFreeCursor, cursor);
  core.mem_w32(cursor, record);
}

// The key a packet stored under the producer's scope for `object` carries.
psx::present::RecordKey keyOf(Core &core, std::uint32_t object) {
  {
    const psx::present::EmissionScope::Guard scope(core.emission, kProducer, object, 0u);
    core.mem_w32(kPacket, 0x00FFFFFFu);
    core.mem_w32(kPacket + 4u, 0x20000000u);
  }
  const auto key = core.emission.keyFor(kPacket);
  return key.value_or(psx::present::RecordKey{});
}

void testPop(Core &core) {
  RenderRecordPool pool;
  seed(core);
  check(pool.object(kRecordA) == incarnationObject(kRecordA, 0u), "a record never popped is its first life");
  check(pool.allocate(core) == kRecordA, "the top of the stack is popped first");
  check(core.mem_r16(RenderRecordPool::kFreeCount) == 1u, "the count drops");
  check(core.mem_r32(RenderRecordPool::kFreeCursor) == kStack + 4u, "the cursor advances");
  check(pool.object(kRecordA) == incarnationObject(kRecordA, 1u), "a pop begins the next life");
  check(pool.object(kRecordB) == incarnationObject(kRecordB, 0u), "other records keep theirs");
  check(pool.allocate(core) == kRecordB, "then the next");
  check(pool.allocate(core) == 0u, "an empty stack pops nothing");
  check(static_cast<std::int16_t>(core.mem_r16(RenderRecordPool::kFreeCount)) == 0, "an empty stack keeps its count");
  check(pool.object(kRecordB) == incarnationObject(kRecordB, 1u), "a failed pop begins no life");
}

void testGuestRegisters(Core &core) {
  RenderRecordPool pool;
  seed(core);
  core.r[3] = 0xDEADBEEFu;
  pool.allocateForGuest(core);
  check(core.r[2] == kRecordA, "v0 is the record");
  check(core.r[3] == kStack + 4u, "v1 is the advanced cursor");
  check(core.r[4] == 0x800E0000u && core.r[6] == 0x800F0000u, "a0/a2 hold the stack address halves");
  check(core.r[5] == 2u, "a1 is the count before the pop");

  core.mem_w16(RenderRecordPool::kFreeCount, 0u);
  core.r[3] = 0xDEADBEEFu;
  pool.allocateForGuest(core);
  check(core.r[2] == 0u && core.r[5] == 0u, "an empty stack returns 0 with a1 = count");
  check(core.r[3] == 0xDEADBEEFu, "an empty stack leaves v1");
}

void testRespawnKeys(Core &core) {
  RenderRecordPool pool;
  seed(core);
  const std::uint32_t record = pool.allocate(core);
  const auto firstLife = keyOf(core, pool.object(record));
  check(keyOf(core, pool.object(record)) == firstLife, "a live record keeps its key from frame to frame");
  release(core, record);
  check(pool.allocate(core) == record, "despawn then spawn reuses the record");
  const auto secondLife = keyOf(core, pool.object(record));
  check(!(secondLife == firstLife), "the next object on the record carries another key");
  check(secondLife.producer == firstLife.producer && secondLife.element == firstLife.element,
        "only the object identity changes");
}

} // namespace

int main() {
  tomba::test::StubRuntime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  testPop(core);
  testGuestRegisters(core);
  testRespawnKeys(core);
  lucent::info("render-record-pool-test", "checked={} failed={}", checked, failed);
  return failed == 0 ? 0 : 1;
}
