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
constexpr std::uint32_t kArea8Tick = 0x8010B328u;
constexpr std::uint32_t kArea8FrameSize = 0x30u;
constexpr std::uint32_t kArea14Tick = 0x8010967Cu;
constexpr std::uint32_t kArea14FrameSize = 0x18u;

constexpr std::uint32_t kGameOverLatch = 0x800BF80Du;
constexpr std::uint32_t kWaterSound = 0x800BF81Eu;
constexpr std::uint32_t kAreaEventFlags = 0x800BFE56u;
constexpr std::uint32_t kStandOnObject = 0x1F80027Bu;
constexpr std::uint32_t kForcedFloor = 0x1F800253u; // 2: the floor reads as water kind 0x8900
constexpr std::uint32_t kEventLock = 0x800BF873u;
constexpr std::uint32_t kAirLock = 0x1F800137u; // nonzero: no water start or fall this frame

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

// The pieces the areas' ticks share.
class AreaWaterTick : public TombaStep {
protected:
  using TombaStep::TombaStep;

  void hazard(std::uint8_t substate) {
    setU8(0, 3);
    setU8(kGroup, kHazardGroup);
    setU8(kMode, substate);
    setU8(kSubstate, 0);
    setU8(kPhase, 0);
  }

  // A06 and A0E: the swim start, or the dive out of mode 6.
  void swimStart() {
    if (u8(kMode) == 6u) {
      setU16(kFloorAttribute, 0);
      setU8(kMode, kDiveMode);
      setU8(0x149u, (u8(0x149u) & 1u) | 2u);
    } else {
      setU8(kMode, kSwimMode);
      setU8(kSubstate, 0);
    }
    setU8(kGroup, 1);
    setU8(kPhase, 0);
    if (u8(kApexLatch) == 1u) {
      setU8(kApexLatch, 0);
    }
  }

  bool onStandingObject() const {
    return u8(0x146u) == 4u && core_.mem_r8(kStandOnObject) != 0u;
  }
};

// A06 FUN_8010B334.
class Area6Tick : public AreaWaterTick {
public:
  explicit Area6Tick(Core &core) : AreaWaterTick(core, "WaterEntry::area6Tick") {}

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
      setU16(kFloorAttribute, 0);
      hazard(kDeathFloorSubstate);
    } else if (type == kThrowFloor) {
      throwBack();
    }
  }

private:
  static constexpr std::uint32_t kFloorTrigger = 0x8010B1F8u;    // (Tomba)
  static constexpr std::uint32_t kThrowBackPoints = 0x801422A0u; // {x, y, z, heading << 8 | side} near, far

  void water() {
    const std::uint8_t mode = u8(kMode);
    if (mode == 0x18u || u8(kGroup) >= 2u) {
      return;
    }
    if ((core_.mem_r16(kAreaEventFlags) & 0x40u) == 0u || !has(SuitAbility::Swim)) {
      hazard(kWaterFallSubstate);
      return;
    }
    if (u8(0x181u) != 0u || onStandingObject()) {
      return;
    }
    setU8(0x16Au, 0);
    swimStart();
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

// A08 FUN_8010B328.
class Area8Tick : public AreaWaterTick {
public:
  explicit Area8Tick(Core &core) : AreaWaterTick(core, "WaterEntry::area8Tick") {}

  void run() {
    if (core_.mem_r8(kEventLock) != 0u || core_.mem_r8(kGameOverLatch) != 0u || u8(0) == 2u) {
      return;
    }
    if (u8(0x181u) != 0u && (u8(kGroup) != 1u || u8(kMode) >= 2u)) {
      return;
    }
    call(kFloorTrigger, tomba_);
    const std::uint16_t attribute = u16(kFloorAttribute);
    if ((attribute & kHazardBit) == 0u) {
      return;
    }
    core_.mem_w8(kWaterSound, 0);
    const std::uint16_t type = attribute & 0xF00u;
    if (type == kWaterFloor || type == kWaterFloor2) {
      water();
    } else if (type == kDeathFloor && core_.mem_r8(kAirLock) == 0u) {
      hazard(kDeathFloorSubstate);
    }
  }

private:
  static constexpr std::uint32_t kFloorTrigger = 0x8010B1E8u; // (Tomba)
  static constexpr std::uint32_t kSurfaceProbe = 0x8012DE84u; // (Tomba, y): nonzero on the water surface
  static constexpr std::uint32_t kSurfaceY = 0x1F80019Eu;
  static constexpr std::uint32_t kProbedSurfaceY = 0x1F8001A4u;
  static constexpr std::uint8_t kClimbMode = 0x2Cu;
  static constexpr std::uint8_t kHangMode = 0x2Au;

  void water() {
    if (u8(kGroup) == 2u || (u8(kAirState) & 1u) != 0u) {
      return;
    }
    if (!has(SuitAbility::Swim)) {
      if (core_.mem_r8(kAirLock) == 0u) {
        setU16(kFloorAttribute, 0);
        hazard(kWaterFallSubstate);
      }
      return;
    }
    if (onStandingObject() || u8(kMode) == kClimbMode) {
      return;
    }
    if (result(kSurfaceProbe, tomba_, static_cast<std::uint32_t>(static_cast<std::int32_t>(s16(0x32u)))) != 0u) {
      core_.mem_w16(kSurfaceY, core_.mem_r16(kProbedSurfaceY));
    }
    if (core_.mem_r8(kAirLock) == 0u) {
      setU8(kGroup, 1);
    }
    const std::uint8_t mode = u8(kMode);
    if (mode == kHangMode) {
      setU8(0x181u, 1);
      return;
    }
    if (u8(kHold) != 0u && mode == 3u) {
      setU8(kLanded, 1);
      setU8(0x181u, 1);
    } else {
      if (u8(kApexLatch) == 1u) {
        setU8(kApexLatch, 0);
      }
      if (mode == 6u) {
        setU8(kMode, kDiveMode);
        setU8(0x149u, (u8(0x149u) & 1u) | 2u);
      } else {
        setU8(kMode, kSwimMode);
        setU8(kSubstate, 0);
      }
    }
    setU8(kPhase, 0);
    if (u8(0x146u) != 0u || u8(kAirState) == 0u) {
      setU16(kVelocityY, 0);
    }
    setU8(kAirState, 0);
  }
};

// A0E FUN_8010967C.
class Area14Tick : public AreaWaterTick {
public:
  explicit Area14Tick(Core &core) : AreaWaterTick(core, "WaterEntry::area14Tick") {}

  void run() {
    if (core_.mem_r8(kGameOverLatch) != 0u) {
      return;
    }
    if (core_.mem_r32(kRideState) == 0x80u) {
      clampToRide();
    }
    const std::uint16_t attribute = u16(kFloorAttribute);
    if ((attribute & kHazardBit) == 0u) {
      return;
    }
    core_.mem_w8(kWaterSound, 0);
    const std::uint16_t type = attribute & 0xF00u;
    if (type == kWaterFloor || type == kWaterFloor2) {
      if (u8(kGroup) >= 2u) {
        return;
      }
      if (!has(SuitAbility::Swim)) {
        hazard(kWaterFallSubstate);
        return;
      }
      if (onStandingObject() || u8(0x181u) != 0u) {
        return;
      }
      swimStart();
    } else if (type == kDeathFloor) {
      setU16(kFloorAttribute, 0);
      hazard(kDeathFloorSubstate);
    }
  }

private:
  static constexpr std::uint32_t kRideState = 0x800BF850u;
  static constexpr std::uint32_t kRideActor = 0x1F800260u;
  static constexpr std::uint32_t kSetHeading = 0x80022D08u; // (actor, 0, 1, 1)

  // Held at the ridden actor's X plus his radius, facing along his heading.
  void clampToRide() {
    const std::uint32_t ride = core_.mem_r32(kRideActor);
    if (ride == 0u) {
      return;
    }
    const std::uint16_t rideX = core_.mem_r16(ride + 0x2Cu);
    const std::uint16_t radius = u16(0x80u);
    if (s16(0x2Eu) > static_cast<std::int16_t>(rideX) + static_cast<std::int16_t>(radius)) {
      return;
    }
    setU16(0x2Eu, rideX + radius);
    const std::uint32_t heading = u8(0x2Au) < 0xFu ? static_cast<std::uint32_t>(s16(0x140u) + 0x800) >> 4u
                                                   : static_cast<std::uint32_t>(u16(0x140u)) >> 4u;
    setU8(0x2Bu, heading);
    call(kSetHeading, tomba_, 0u, 1u, 1u);
  }
};

} // namespace

void WaterEntry::area6Tick(Core *core) {
  psx::cpu::PreservedReturnAddress returnAddress(*core);
  psx::cpu::GuestFrameScope frame(*core, core->r[psx::cpu::kSp] - kArea6FrameSize);
  Area6Tick(*core).run();
}

void WaterEntry::area8Tick(Core *core) {
  psx::cpu::PreservedReturnAddress returnAddress(*core);
  psx::cpu::GuestFrameScope frame(*core, core->r[psx::cpu::kSp] - kArea8FrameSize);
  Area8Tick(*core).run();
}

void WaterEntry::area14Tick(Core *core) {
  psx::cpu::PreservedReturnAddress returnAddress(*core);
  psx::cpu::GuestFrameScope frame(*core, core->r[psx::cpu::kSp] - kArea14FrameSize);
  Area14Tick(*core).run();
}

void WaterEntry::registerAreaTicks(native::NativeOverrideCatalog &natives) {
  natives.declareOverlay("A06", kArea6Tick, "WaterEntry::area6Tick", &WaterEntry::area6Tick);
  natives.declareOverlay("A08", kArea8Tick, "WaterEntry::area8Tick", &WaterEntry::area8Tick);
  natives.declareOverlay("A0E", kArea14Tick, "WaterEntry::area14Tick", &WaterEntry::area14Tick);
}

} // namespace tomba
