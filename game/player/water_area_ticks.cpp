// The overlay areas' water against the swim bit: their floor ticks and A08's waterfall contact.
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
constexpr std::uint32_t kWaterfallContact = 0x8010D334u;
constexpr std::uint32_t kWaterfallContactFrameSize = 0x28u;

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

// A08 FUN_8010D334 (a0 Tomba, a1 the waterfall): the contact by the waterfall's shape (byte 3). A swimmer is lifted
// up the falling water; anyone else is pushed out of it and knocked back (hazard substate 2).
class WaterfallContact : public TombaStep {
public:
  explicit WaterfallContact(Core &core)
      : TombaStep(core, "WaterEntry::waterfallContact"), fall_(core.r[psx::cpu::kA1]) {}

  void run() {
    const bool locked = core_.mem_r8(kEventLock) != 0u;
    switch (core_.mem_r8(fall_ + 3u)) {
    case 0:
    case 5:
      if (locked) {
        call(kEventContact, tomba_, fall_);
      }
      return;
    case 1:
      if (!locked) {
        cylinder();
      }
      return;
    case 2:
      if (!locked && u8(0x181u) == 0u && result(kBoxContact, tomba_, fall_) != 0u) {
        heading();
      }
      return;
    case 3:
      if (!locked && u8(0x181u) == 0u && result(kSpanContact, tomba_, fall_) != 0u) {
        span();
      }
      return;
    case 4:
      if (!locked && u8(0x181u) == 0u && result(kBoxContact, tomba_, fall_) != 0u) {
        box();
      }
      return;
    default:
      return;
    }
  }

private:
  static constexpr std::uint32_t kEventContact = 0x8001EC3Cu; // (Tomba, actor)
  static constexpr std::uint32_t kBoxContact = 0x80022F04u;   // (Tomba, actor): nonzero on contact
  static constexpr std::uint32_t kSpanContact = 0x8001F830u;  // (Tomba, actor): nonzero on contact
  static constexpr std::uint32_t kSqrt = 0x80084080u;         // Math_sqrtLzc
  static constexpr std::uint32_t kAtan2 = 0x80085690u;        // Trig_ratan2 (y, x)
  static constexpr std::uint32_t kCos = 0x80083F50u;          // Trig_rcos
  static constexpr std::uint32_t kSin = 0x80083E80u;          // Trig_rsin
  static constexpr std::uint32_t kContactAngle = 0x1F80009Cu;
  static constexpr std::uint8_t kKnockBackSubstate = 2u;

  std::uint16_t fallU16(std::uint32_t field) const {
    return core_.mem_r16(fall_ + field);
  }
  std::int32_t fallS16(std::uint32_t field) const {
    return static_cast<std::int16_t>(core_.mem_r16(fall_ + field));
  }
  std::int32_t reach() const {
    return s16(0x80u) + fallS16(0x80u);
  }
  std::int32_t trig(std::uint32_t function, std::int32_t angle) {
    return static_cast<std::int32_t>(result(function, static_cast<std::uint32_t>(angle)));
  }
  // Tomba's feet over the fall's top, with the fall's own height when `withFallHeight`.
  std::uint32_t top(bool withFallHeight) const {
    std::uint32_t y = fallU16(0x32u) + (u16(0x86u) - u16(0x84u));
    if (withFallHeight) {
      y += fallU16(0x86u) - fallU16(0x84u);
    }
    return y;
  }

  // Shape 1: a cylinder of the summed radii around the fall.
  void cylinder() {
    const std::uint32_t dx = u16(0x2Eu) - fallU16(0x2Eu);
    const std::uint32_t dz = u16(0x36u) - fallU16(0x36u);
    const std::int32_t sdx = static_cast<std::int16_t>(dx);
    const std::int32_t sdz = static_cast<std::int16_t>(dz);
    const std::uint32_t squared = static_cast<std::uint32_t>(sdx * sdx) + static_cast<std::uint32_t>(sdz * sdz);
    if (reach() < static_cast<std::int32_t>(result(kSqrt, squared) & 0xFFFFu)) {
      return;
    }
    const bool swims = has(SuitAbility::Swim);
    const std::uint32_t height = (u16(0x32u) - fallU16(0x32u) + u16(0x84u) + fallU16(0x84u)) & 0xFFFFu;
    if (s16(0x86u) + fallS16(swims ? 0x84u : 0x86u) < static_cast<std::int32_t>(height)) {
      return;
    }
    const std::int32_t angle = trig(kAtan2, -sdz, sdx);
    core_.mem_w32(kContactAngle, static_cast<std::uint32_t>(angle));
    if (swims) {
      if (u8(0x164u) == 0u) {
        halveSpeed();
        rise<6>(top(false), true);
      }
      return;
    }
    if ((u8(0) & 4u) == 0u) {
      pushOut(angle);
    }
    knockBack();
  }

  // Shape 2: a box; Tomba is first set at the summed radii along his own heading.
  void heading() {
    setU8(0x5Fu, 2);
    const std::int32_t along = s16(0x140u) + 0x800;
    setU16(0x2Eu, fallU16(0x2Eu) + static_cast<std::uint32_t>((trig(kCos, along) * reach()) >> 12));
    const std::int32_t across = s16(0x140u) + 0x800;
    setU16(0x36u, fallU16(0x36u) - static_cast<std::uint32_t>((trig(kSin, across) * reach()) >> 12));
    if (has(SuitAbility::Swim)) {
      rise<0x10>(top(true), false);
      return;
    }
    knockBack();
  }

  // Shape 3: a span along the path.
  void span() {
    if (!has(SuitAbility::Swim)) {
      knockBack();
      return;
    }
    if (u8(0x164u) == 0u) {
      halveSpeed();
      rise<0x10>(top(true), false);
    }
  }

  // Shape 4: a box; Tomba is pushed out along the last contact angle.
  void box() {
    if (has(SuitAbility::Swim)) {
      if (u8(0x164u) == 0u) {
        halveSpeed();
        rise<6>(top(true), true);
      }
      return;
    }
    if ((u8(0) & 4u) == 0u) {
      pushOut(static_cast<std::int32_t>(core_.mem_r32(kContactAngle)));
    }
    knockBack();
  }

  std::int32_t trig(std::uint32_t function, std::int32_t y, std::int32_t x) {
    return static_cast<std::int32_t>(result(function, static_cast<std::uint32_t>(y), static_cast<std::uint32_t>(x)));
  }

  void halveSpeed() {
    setU16(0x44u, static_cast<std::uint32_t>(s16(0x44u) >> 1));
  }

  // Up toward `target` by at most `step` a frame; in the air his fall is slowed (`slow`) or stopped.
  template <std::int32_t kStep> void rise(std::uint32_t target, bool slow) {
    const std::uint16_t y = u16(0x32u);
    if (static_cast<std::int16_t>(target) <= s16(0x32u)) {
      return;
    }
    std::int32_t climb = static_cast<std::int16_t>(target - y);
    if (climb > kStep) {
      climb = kStep;
    }
    setU16(0x32u, y + static_cast<std::uint32_t>(climb));
    core_.mem_w8(kForcedFloor, 4);
    if (u8(kAirState) != 1u) {
      return;
    }
    setU16(kVelocityY, slow ? u16(kVelocityY) + 0x200u : 0u);
  }

  void pushOut(std::int32_t angle) {
    setU16(0x2Eu, fallU16(0x2Eu) + static_cast<std::uint32_t>((trig(kCos, angle) * reach()) >> 12));
    const auto again = static_cast<std::int32_t>(core_.mem_r32(kContactAngle));
    setU16(0x36u, fallU16(0x36u) - static_cast<std::uint32_t>((trig(kSin, again) * reach()) >> 12));
  }

  void knockBack() {
    if (core_.mem_r8(kAirLock) != 0u || (u8(0) & 6u) != 0u) {
      return;
    }
    setU8(kGroup, kHazardGroup);
    setU8(kMode, kKnockBackSubstate);
    setU8(0, 3);
    setU8(kSubstate, 0);
    setU16(0x172u, 0x78u);
    setU8(0x2Bu, static_cast<std::uint32_t>(static_cast<std::int32_t>(core_.mem_r32(kContactAngle)) >> 4));
  }

  std::uint32_t fall_;
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

void WaterEntry::waterfallContact(Core *core) {
  psx::cpu::PreservedReturnAddress returnAddress(*core);
  psx::cpu::GuestFrameScope frame(*core, core->r[psx::cpu::kSp] - kWaterfallContactFrameSize);
  WaterfallContact(*core).run();
}

void WaterEntry::registerAreaTicks(native::NativeOverrideCatalog &natives) {
  natives.declareOverlay(
      "A06", kArea6Tick, "WaterEntry::area6Tick", &WaterEntry::area6Tick, psx::cpu::NativeReturn::Void);
  natives.declareOverlay(
      "A08", kArea8Tick, "WaterEntry::area8Tick", &WaterEntry::area8Tick, psx::cpu::NativeReturn::Void);
  natives.declareOverlay("A08",
                         kWaterfallContact,
                         "WaterEntry::waterfallContact",
                         &WaterEntry::waterfallContact,
                         psx::cpu::NativeReturn::Void);
  natives.declareOverlay(
      "A0E", kArea14Tick, "WaterEntry::area14Tick", &WaterEntry::area14Tick, psx::cpu::NativeReturn::Void);
}

} // namespace tomba
