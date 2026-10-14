#pragma once
#include "core.h"
#include "sched/task_slot_layout.h"

namespace tomba {

// A developer command is legal only from the field: the GAME stage owns the area machine and the inventory.
class DevGate {
public:
  static bool inGameStage(Core &core) {
    return core.mem_r32(sched::taskSlotBase(0) + sched::kTaskEntry) == sched::kStageGameEntry;
  }
};

} // namespace tomba
