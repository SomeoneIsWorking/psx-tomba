#include "debug/dev_control.h"

#include "core.h"
#include "debug/dev_args.h"
#include "entry/tomba_session.h"

#include <string_view>

namespace tomba {

bool DevControl::handle(Core &core, const char *line, std::string &reply) {
  const auto words = DevArgs::words(line);
  const std::string_view command = words.empty() ? std::string_view{} : words[0];
  if (command == "warp") {
    reply = warp_.arm(core, line);
  } else if (command == "items") {
    reply = items_.arm(core, line);
  } else if (command == "flag") {
    reply = flags_.arm(core, line);
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
