#include "debug/dev_control.h"

#include "core.h"
#include "entry/session.h"

#include <string_view>

namespace tomba {

bool DevControl::handle(Core &core, const psx::debug::CommandArgs &args, std::string &reply) {
  const std::string_view command = args.word(0);
  if (command == "warp") {
    reply = warp_.arm(core, args);
  } else if (command == "items") {
    reply = items_.arm(core, args);
  } else if (command == "flag") {
    reply = flags_.arm(core, args);
  } else {
    return false;
  }
  return true;
}

void DevControl::giveAndFlag(Core &core, const Inventory::ItemGrantArgs &grant) {
  inv(&core).giveAndFlag(grant);
}

void DevControl::applyArmed(Core &core, uint32_t frame) {
  warp_.applyArmed(core, frame);
  items_.applyArmed(core, &DevControl::giveAndFlag, frame);
  flags_.applyArmed(core, frame);
}

} // namespace tomba
