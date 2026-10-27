#include "debug/dev_items.h"

#include "core.h"
#include "items/inventory_layout.h"
#include "items/item_grant.h"

#include <lucent/log.h>

namespace tomba {

std::string DevItems::armAll() {
  request_ = Request::All;
  return "ok: grant all items armed";
}

std::string DevItems::armOne(uint32_t item, uint32_t amount) {
  request_ = Request::One;
  item_ = item;
  amount_ = amount;
  return lucent::format("ok: give item {} x{} armed", item_, amount_);
}

void DevItems::applyArmed(Core &core, GiveFn give, uint32_t frame) {
  const Request request = request_;
  request_ = Request::None;
  if (request == Request::All) {
    ItemGrant::all(core);
    lucent::info("items", "all items granted at f{}", frame);
  } else if (request == Request::One) {
    const uint32_t address = inventory_layout::kCounts + item_;
    const uint32_t before = core.mem_r8(address);
    give(core, {.type = item_, .amount = amount_});
    lucent::info("items", "item {} x{} at f{}: count {} -> {}", item_, amount_, frame, before, core.mem_r8(address));
  }
}

} // namespace tomba
