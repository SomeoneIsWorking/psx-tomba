// The overlay areas' floor ticks: each area's own test of a water floor against the swim bit.
#include "guest_abi.h"
#include "overrides/native_override_catalog.h"
#include "player/tomba_step.h"
#include "player/water_entry.h"

#include <cstdint>

namespace tomba {
namespace {

using namespace tomba_field;

constexpr std::uint32_t kArea6Tick = 0x8010B334u;
constexpr std::uint32_t kArea6FrameSize = 0x28u;

constexpr std::uint32_t kGameOverLatch = 0x800BF80Du;
constexpr std::uint32_t kWaterSound = 0x800BF81Eu;
constexpr std::uint32_t kAreaEventFlags = 0x800BFE56u;
constexpr std::uint32_t kStandOnObject = 0x1F80027Bu;
constexpr std::uint32_t kForcedFloor = 0x1F800253u; // 2: the floor reads as water kind 0x8900

constexpr std::uint16_t kNoFruitState = 0x200u;
constexpr std::uint16_t kHazardBit = 0x8000u;
constexpr std::uint16_t kWaterFloor = 0x100u;
constexpr std::uint16_t kWaterFloor2 = 0x200u;
constexpr std::uint16_t kDeathFloor = 0x700u;
constexpr std::uint16_t kThrowFloor = 0x900u;
constexpr std::uint8_t kSwimMode = 0x1Du;
constexpr std::uint8_t kDiveMode = 0x32u;
constexpr std::uint8_t kHazardGroup = 2u;
constexpr std::uint8_t kWaterFallSubstate = 4u;
constexpr std::uint8_t kDeathFloorSubstate = 5u;
constexpr std::uint8_t kThrowBackSubstate = 3u;

// A06 FUN_8010B334.
class Area6Tick : public TombaStep {
public:
  explicit Area6Tick(Core &core) : TombaStep(core, "WaterEntry::area6Tick") {}

  void run() {
    if (u8(0) == 6u || core_.mem_r8(kGameOverLatch) != 0u) {
      return;
    }
    if (core_.mem_r8(kForcedFloor) == 2u) {
      setU16(kFloorAttribute, 0x8900u);
    }
    if ((u16(0x17Eu) & kNoFruitState) == 0u) {
      call(kFloorTrigger, tomba_);
    }
    const std::uint16_t attribute = u16(kFloorAttribute);
    if ((attribute & kHazardBit) == 0u) {
      return;
    }
    core_.mem_w8(kWaterSound, 0);
    const std::uint16_t type = attribute & 0xF00u;
    if (type == kWaterFloor || type == kWaterFloor2) {
      water();
    } else if (type == kDeathFloor) {
      setU8(0, 3);
      setU16(kFloorAttribute, 0);
      hazard(kDeathFloorSubstate);
    } else if (type == kThrowFloor) {
      throwBack();
    }
  }

private:
  static constexpr std::uint32_t kFloorTrigger = 0x8010B1F8u;    // (Tomba)
  static constexpr std::uint32_t kThrowBackPoints = 0x801422A0u; // {x, y, z, heading << 8 | side} near, far

  void hazard(std::uint8_t substate) {
    setU8(kGroup, kHazardGroup);
    setU8(kMode, substate);
    setU8(kSubstate, 0);
    setU8(kPhase, 0);
  }

  void water() {
    const std::uint8_t mode = u8(kMode);
    if (mode == 0x18u || u8(kGroup) >= 2u) {
      return;
    }
    if ((core_.mem_r16(kAreaEventFlags) & 0x40u) == 0u || !has(SuitAbility::Swim)) {
      setU8(0, 3);
      hazard(kWaterFallSubstate);
      return;
    }
    if (u8(0x181u) != 0u || (u8(0x146u) == 4u && core_.mem_r8(kStandOnObject) != 0u)) {
      return;
    }
    if (mode == 6u) {
      setU16(kFloorAttribute, 0);
      setU8(kMode, kDiveMode);
      setU8(0x149u, (u8(0x149u) & 1u) | 2u);
    } else {
      setU8(kMode, kSwimMode);
      setU8(kSubstate, 0);
    }
    setU8(kGroup, 1);
    setU8(0x16Au, 0);
    setU8(kPhase, 0);
    if (u8(kApexLatch) == 1u) {
      setU8(kApexLatch, 0);
    }
  }

  // Thrown back to the near or far point, by Z against 15000.
  void throwBack() {
    setU8(kMode, kThrowBackSubstate);
    setU16(kFloorAttribute, 0);
    setU8(kGroup, kHazardGroup);
    setU8(kSubstate, 0);
    setU8(kPhase, 0);
    setU8(0, u8(0) | 4u);
    const std::uint32_t point = kThrowBackPoints + (s16(kZ + 2u) >= 15000 ? 8u : 0u);
    setU16(0x7Au, core_.mem_r16(point));
    setU16(0x7Cu, core_.mem_r16(point + 2u));
    setU16(0x7Eu, core_.mem_r16(point + 4u));
    const std::uint16_t facing = core_.mem_r16(point + 6u);
    setU8(0x2Au, facing >> 8);
    setU8(kSide, facing & 1u);
  }
};

} // namespace

void WaterEntry::area6Tick(Core *core) {
  psx::cpu::PreservedReturnAddress returnAddress(*core);
  psx::cpu::GuestFrameScope frame(*core, core->r[psx::cpu::kSp] - kArea6FrameSize);
  Area6Tick(*core).run();
}

void WaterEntry::registerAreaTicks(native::NativeOverrideCatalog &natives) {
  natives.declareOverlay("A06", kArea6Tick, "WaterEntry::area6Tick", &WaterEntry::area6Tick);
}

} // namespace tomba
