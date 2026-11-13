#include "player/hazard_fall.h"

#include "core.h"
#include "entry/mod_settings.h"
#include "guest_call.h"
#include "native_dispatch.h"
#include "overrides/native_override_catalog.h"

namespace tomba {
namespace {

constexpr std::uint32_t kDeathFloorStep = 0x800678E8u;
// FUN_8006762C: the water fall's substate.
constexpr std::uint32_t kWaterFallStep = 0x8006762Cu;
constexpr std::uint32_t kSubstate = 5u; // G+5, the hazard state's substate
constexpr std::uint8_t kWaterFall = 4;

} // namespace

void HazardFall::deathFloorStep(Core *core) {
  if (!ModSettings::pitVoidOut()) {
    psx::cpu::callOriginalToReturn(
        *core, kDeathFloorStep, psx::cpu::ExecutionBudget::currentTurn(*core), "HazardFall::deathFloorStep");
    return;
  }
  const std::uint32_t tomba = core->r[4];
  core->mem_w8(tomba + kSubstate, kWaterFall);
  psx::cpu::callGuestNow(*core, "HazardFall::deathFloorStep", kWaterFallStep, tomba);
}

void HazardFall::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declare(kDeathFloorStep, "HazardFall::deathFloorStep", &HazardFall::deathFloorStep);
}

} // namespace tomba
