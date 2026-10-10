#include "debug/dev_control.h"

#include "core.h"
#include "entry/game_ctx.h"

#include <cstring>

namespace tomba {

bool DevControl::handle(Core &core, const char *command, const char *line, std::string &reply) {
  if (std::strcmp(command, "warp") == 0) {
    reply = warp_.arm(core, line);
  } else if (std::strcmp(command, "items") == 0) {
    reply = items_.arm(core, line);
  } else if (std::strcmp(command, "flag") == 0) {
    reply = flags_.arm(core, line);
  } else {
    return false;
  }
  return true;
}

void DevControl::giveAndFlag(Core &core, uint32_t item, uint32_t amount) {
  inv(&core).giveAndFlag(item, amount);
}

void DevControl::applyArmed(Core &core, uint32_t frame) {
  warp_.applyArmed(core, frame);
  items_.applyArmed(core, &DevControl::giveAndFlag, frame);
  flags_.applyArmed(core, frame);
}

} // namespace tomba
