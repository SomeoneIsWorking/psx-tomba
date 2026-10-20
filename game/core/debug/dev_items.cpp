#include "debug/dev_items.h"

#include "core.h"
#include "debug/dev_gate.h"
#include "items/inventory_layout.h"
#include "items/item_grant.h"

#include <lucent/log.h>

namespace tomba {

std::string DevItems::arm(Core &core, const psx::debug::CommandArgs &args) {
  const std::optional<uint32_t> itemWord = args.number(1);
  const std::optional<uint32_t> amountWord = args.size() == 2 ? 1u : args.number(2);
  const bool all = args.size() == 2 && args.word(1) == "all";
  const bool one = (args.size() == 2 || args.size() == 3) && itemWord && amountWord;
  if (!all && !one) {
    return "usage: items all | items <id 0..255> [amount 1..99]";
  }
  if (!DevGate::inGameStage(core)) {
    return "refused: enter the GAME field before changing inventory";
  }
  if (all) {
    request_ = Request::All;
    return "ok: grant all items armed";
  }
  const uint32_t item = *itemWord;
  const uint32_t amount = *amountWord;
  if (item > kMaxItemId || amount < 1 || amount > kMaxAmount) {
    return "refused: item id is 0..255 and amount is 1..99";
  }
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
