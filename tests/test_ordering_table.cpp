// OrderingTable and PacketPool: the guest's AddPrim link, chain splice, pool bump and depth buckets.
#include "game.h"
#include "guest_ordering_table.h"
#include "stub_runtime.h"

#include <cstdint>
#include <lucent/log.h>
#include <memory>

namespace {

using tomba2::render::OrderingTable;
using tomba2::render::PacketPool;

constexpr std::uint32_t kOt = 0x801B0000u;
constexpr std::uint32_t kPool = 0x801A0000u;
constexpr std::uint32_t kChainEnd = 0x00FFFFFFu;

int failed = 0;
int checked = 0;

void check(bool condition, const char *name) {
  ++checked;
  if (!condition) {
    ++failed;
    lucent::error("ordering-table-test", "failed: {}", name);
  }
}

void testLink(Core &core) {
  core.mem_w32(OrderingTable::kBasePointer, kOt);
  core.mem_w32(kOt + 7u * 4u, kChainEnd);
  const auto ot = OrderingTable::active(core);
  check(ot.base() == kOt && ot.slot(7u) == kOt + 28u, "the active OT is the published base");

  const std::uint32_t first = ot.link(kPool, 9u, 7u);
  check(first == (kChainEnd | 0x09000000u) && core.mem_r32(kPool) == first, "the tag is the old head with the count");
  check(core.mem_r32(kOt + 28u) == kPool, "the bucket head is the linked packet");

  const std::uint32_t second = ot.link(kPool + 40u, 2u, 7u);
  check(second == (kPool | 0x02000000u) && ot.head(7u) == kPool + 40u, "a second packet is drawn before the first");

  const OrderingTable handed(core, kOt + 0x100u);
  check(handed.slot(1u) == kOt + 0x104u, "an OT handed over in a register keeps its own base");
}

void testChainToHead(Core &core) {
  const auto ot = OrderingTable::active(core);
  constexpr std::uint32_t kLast = kPool + 0x200u;
  ot.setHead(OrderingTable::kFarthestBucket, 0x80123456u);
  core.mem_w32(kLast, 0x03ABCDEFu);
  ot.chainToHead(kLast, OrderingTable::kFarthestBucket);
  check(core.mem_r32(kLast) == 0x83123456u, "a chain's last tag keeps its top byte and takes the head");
  check(ot.head(OrderingTable::kFarthestBucket) == 0x80123456u, "splicing a chain leaves the head alone");
}

void testPool(Core &core) {
  const PacketPool pool(core);
  pool.setCursor(kPool);
  check(pool.allocate(40u) == kPool && pool.cursor() == kPool + 40u, "allocate returns the cursor and bumps it");
  check(core.mem_r32(PacketPool::kCursor) == kPool + 40u, "the cursor lives in the guest word");
}

void testDepth() {
  check(OrderingTable::compressDepth(1023) == 1023, "depths below 1024 map to themselves");
  check(OrderingTable::compressDepth(1024) == 1024 / 2 + 0x200, "the first band halves and offsets");
  check(OrderingTable::compressDepth(4096) == (4096 >> 4) + 4 * 0x200, "band four shifts by four");
  check(!OrderingTable::inDepthRange(3) && OrderingTable::inDepthRange(4) && OrderingTable::inDepthRange(0x7FF) &&
            !OrderingTable::inDepthRange(0x800) && !OrderingTable::inDepthRange(-1),
        "the closed range keeps [4, 0x7FF]");
  check(!OrderingTable::inDepthRangeExclusive(4) && OrderingTable::inDepthRangeExclusive(5) &&
            OrderingTable::inDepthRangeExclusive(0x7FE) && !OrderingTable::inDepthRangeExclusive(0x7FF),
        "the exclusive range drops both end buckets");
}

} // namespace

int main() {
  tomba::test::StubRuntime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  testLink(core);
  testChainToHead(core);
  testPool(core);
  testDepth();
  lucent::info("ordering-table-test", "checked={} failed={}", checked, failed);
  return failed == 0 ? 0 : 1;
}
