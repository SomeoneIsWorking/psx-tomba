#include "debug/dev_control.h"

#include "core.h"
#include "entry/session.h"

namespace tomba {

void DevControl::giveAndFlag(Core &core, const Inventory::ItemGrantArgs &grant) {
  inv(&core).giveAndFlag(grant);
}

void DevControl::applyArmed(Core &core, uint32_t frame) {
  warp_.applyArmed(core, frame);
  items_.applyArmed(core, &DevControl::giveAndFlag, frame);
  flags_.applyArmed(core, frame);
}

} // namespace tomba
