#pragma once
#include "core.h"

#include <cstdint>

namespace tomba {

// A developer command is legal only from the field: the GAME stage owns the area machine and the inventory.
class DevGate {
public:
  inline static constexpr uint32_t kStagePointer = 0x801FE00Cu;
  inline static constexpr uint32_t kGameStage = 0x8010637Cu;

  static bool inGameStage(Core &core) {
    return core.mem_r32(kStagePointer) == kGameStage;
  }
};

} // namespace tomba
