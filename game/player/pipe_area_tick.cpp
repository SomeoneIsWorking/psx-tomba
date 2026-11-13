#include "player/pipe_area_tick.h"

#include "guest_abi.h"
#include "overrides/native_override_catalog.h"
#include "player/tomba_step.h"

#include <cstdint>

namespace tomba {
namespace {

using namespace tomba_field;

constexpr std::uint32_t kTick = 0x8010C7BCu;
constexpr std::uint32_t kTickFrameSize = 0x18u;

constexpr std::uint32_t kGameOverLatch = 0x800BF80Du;
constexpr std::uint32_t kEntry = 0x800BF871u;
constexpr std::uint32_t kAreaEventFlags = 0x800BFE56u;
constexpr std::uint32_t kExitKind = 0x800BF839u;
constexpr std::uint32_t kFlutterGate = 0x800BF8BEu; // 0xFF: no push in the column
constexpr std::uint32_t kExitByFloor = 0x8010C6ECu; // (Tomba): picks the exit target by G+0x2A

constexpr std::uint8_t kLastEntry = 0xEu;
constexpr std::uint16_t kExitEvent = 2u;
constexpr std::uint16_t kHazardBit = 0x8000u;
constexpr std::uint16_t kDeathFloor = 0x700u;
constexpr std::int32_t kExitCeiling = -0x8FB;
constexpr std::int32_t kColumnTop = -0x1A8F;
constexpr std::uint32_t kColumnLeft = 0x28B5u;
constexpr std::uint32_t kColumnWidth = 0x4B5u;
constexpr std::uint16_t kColumnPush = 0x20u;

class Tick : public TombaStep {
public:
  explicit Tick(Core &core) : TombaStep(core, "PipeAreaTick::tick") {}

  void run() {
    if (core_.mem_r8(kGameOverLatch) != 0u || core_.mem_r8(kEntry) > kLastEntry) {
      return;
    }
    const std::uint16_t attribute = u16(kFloorAttribute);
    const bool hazard = (attribute & kHazardBit) != 0u;
    if ((core_.mem_r16(kAreaEventFlags) & kExitEvent) != 0u) {
      if (core_.mem_r8(kExitKind) != 0u) {
        return;
      }
      if (hazard) {
        if ((attribute & 0xF00u) != kDeathFloor) {
          return;
        }
        if (u8(0x164u) != 1u) {
          fall();
          return;
        }
      } else if (s16(kY + 2u) < kExitCeiling) {
        return;
      }
      call(kExitByFloor, tomba_);
      return;
    }
    if (hazard) {
      if ((attribute & 0xF00u) == kDeathFloor) {
        fall();
      }
      return;
    }
    if (core_.mem_r8(kFlutterGate) == 0xFFu || !has(SuitAbility::Flutter) || s16(kY + 2u) < kColumnTop ||
        static_cast<std::uint16_t>(u16(kX + 2u) - kColumnLeft) >= kColumnWidth || u8(0x179u) == 0u) {
      return;
    }
    setU16(kY + 2u, u16(kY + 2u) + kColumnPush);
  }

private:
  // The death floor's fall, hazard substate 5.
  void fall() {
    setU8(0, 3);
    setU8(kGroup, 2);
    setU8(kMode, 5);
    setU8(kSubstate, 0);
    setU8(kPhase, 0);
  }
};

} // namespace

void PipeAreaTick::tick(Core *core) {
  psx::cpu::PreservedReturnAddress returnAddress(*core);
  psx::cpu::GuestFrameScope frame(*core, core->r[psx::cpu::kSp] - kTickFrameSize);
  Tick(*core).run();
}

void PipeAreaTick::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declareOverlay("A01", kTick, "PipeAreaTick::tick", &PipeAreaTick::tick, psx::cpu::NativeReturn::Void);
}

} // namespace tomba
