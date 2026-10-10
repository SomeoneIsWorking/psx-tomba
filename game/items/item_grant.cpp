#include "items/item_grant.h"

#include "core.h"
#include "items/inventory_layout.h"

namespace {

// {first item id, run length, count}. Ids outside the runs are not granted.
struct GrantRun {
  uint32_t first;
  uint32_t length;
  uint8_t count;
};
constexpr GrantRun kGrantAllRuns[] = {
    {0, 13, 1},
    {14, 9, 1},
    {28, 11, 9},
    {40, 1, 9},
    {42, 42, 9},
    {84, 1, 9},
    {86, 1, 9},
    {88, 49, 9},
    {146, 22, 9},
};
constexpr uint8_t kQuestPass = 0xffu;

} // namespace

void ItemGrant::all(Core &core) {
  for (const GrantRun &run : kGrantAllRuns) {
    for (uint32_t id = run.first; id < run.first + run.length; ++id) {
      core.mem_w8(inventory_layout::kCounts + id, run.count);
    }
  }
  core.mem_w8(inventory_layout::kQuestPassCounter, kQuestPass);
}
