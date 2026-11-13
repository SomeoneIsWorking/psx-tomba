#include "player/snow_field_tick.h"

#include "guest_abi.h"
#include "overrides/native_override_catalog.h"
#include "player/tomba_step.h"

#include <cstdint>

namespace tomba {
namespace {

using namespace tomba_field;

constexpr std::uint32_t kTick = 0x8010B024u;
constexpr std::uint32_t kTickFrameSize = 0x30u;

constexpr std::uint32_t kGameOverLatch = 0x800BF80Du;
constexpr std::uint32_t kEntry = 0x800BF871u;
constexpr std::uint32_t kWaterSound = 0x800BF81Eu;
constexpr std::uint32_t kAngleWrapFrames = 0x800BF819u;
constexpr std::uint32_t kAirLock = 0x1F800137u;
constexpr std::uint32_t kExitLock = 0x800BF80Fu;
constexpr std::uint32_t kExitKind = 0x800BF839u;
constexpr std::uint32_t kExitTarget = 0x800BF83Au;
constexpr std::uint32_t kStartExit = 0x8005082Cu; // (0, 0, 0)

constexpr std::uint8_t kExitEntry = 7u;
constexpr std::uint16_t kHazardBit = 0x8000u;
constexpr std::uint16_t kDeathFloor = 0x700u;

class Tick : public TombaStep {
public:
  explicit Tick(Core &core) : TombaStep(core, "SnowFieldTick::tick") {}

  void run() {
    if (core_.mem_r8(kGameOverLatch) != 0u) {
      return;
    }
    const std::uint16_t attribute = u16(kFloorAttribute);
    if (core_.mem_r8(kEntry) == kExitEntry) {
      if ((attribute & 0xF00u) == kDeathFloor) {
        exitArea(attribute);
      }
      return;
    }
    if ((attribute & kHazardBit) != 0u) {
      core_.mem_w8(kWaterSound, 0);
      if ((attribute & 0xF00u) == kDeathFloor) {
        setU8(0, 3);
        setU8(kGroup, 2);
        setU16(kFloorAttribute, 0);
        setU8(kMode, 5);
        setU8(kSubstate, 0);
        setU8(kPhase, 0);
      }
      return;
    }
    if (u8(0x16Au) != 6u && has(SuitAbility::AirControl) && u8(kHold) == 0u && u8(0x2Au) >= 0x27u) {
      core_.mem_w8(kAngleWrapFrames, 4);
    }
  }

private:
  // Entry 7's floor drops out of the area: kind 0x408 for floor bits 12..15 == 1, else 0x402.
  void exitArea(std::uint16_t attribute) {
    core_.mem_w8(kAirLock, 2);
    core_.mem_w8(kExitLock, 2);
    core_.mem_w8(kExitKind, 3);
    call(kStartExit, 0u, 0u, 0u);
    core_.mem_w16(kExitTarget, (attribute >> 12) == 1u ? 0x408u : 0x402u);
  }
};

} // namespace

void SnowFieldTick::tick(Core *core) {
  psx::cpu::PreservedReturnAddress returnAddress(*core);
  psx::cpu::GuestFrameScope frame(*core, core->r[psx::cpu::kSp] - kTickFrameSize);
  Tick(*core).run();
}

void SnowFieldTick::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declareOverlay("A04", kTick, "SnowFieldTick::tick", &SnowFieldTick::tick, psx::cpu::NativeReturn::Void);
}

} // namespace tomba
