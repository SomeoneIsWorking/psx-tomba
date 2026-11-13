#include "player/climb_step.h"

#include "guest_abi.h"
#include "overrides/native_override_catalog.h"
#include "player/tomba_step.h"

#include <cstdint>

namespace tomba {
namespace {

using namespace tomba_field;

constexpr std::uint32_t kStep = 0x80062A30u;
constexpr std::uint32_t kStepFrameSize = 0x28u;

constexpr std::uint32_t kStepTable = 0x800A4700u; // s8: up (-0x10), down; +3 when G+0x165 is set

constexpr std::uint32_t kPlace = 0x156u;
constexpr std::uint32_t kReach = 0x68u;
constexpr std::uint32_t kGrip = 0x148u;
constexpr std::uint32_t kClimbFlags = 0x164u;
constexpr std::uint32_t kClimbTimer = 0x158u;
constexpr std::uint32_t kFastClimb = 0x165u;
constexpr std::uint32_t kSpeedX = 0x44u;
constexpr std::uint32_t kTopExtent = 0x84u;
constexpr std::uint32_t kBottomExtent = 0x86u;

constexpr std::uint8_t kMovingClimbable = 0x17u;
constexpr std::uint8_t kStillState = 2u;
constexpr std::uint32_t kLetGoMode = 3u;
constexpr std::uint32_t kTurnMode = 0x85u;
constexpr std::uint8_t kTurnedAction = 0x35u;
constexpr std::uint16_t kEndDrop = 0x2Cu;

class Step : public TombaStep {
public:
  explicit Step(Core &core)
      : TombaStep(core, "ClimbStep::step"), climbable_(core.r[psx::cpu::kA1]), mode_(core.r[psx::cpu::kA2]) {}

  void run() {
    if (mode_ == kLetGoMode) {
      if (onGround()) {
        letGo();
        stand();
      }
      return;
    }
    const std::uint16_t step = stepSize();
    if ((mode_ & 1u) == 0u) {
      up(step);
      return;
    }
    const auto place = static_cast<std::uint16_t>(u16(kPlace) + step);
    setU16(kPlace, place);
    if ((mode_ & 0x80u) != 0u || movingClimbable()) {
      const std::int32_t lift = s16(0x17Eu) < 0 ? 0x50 : 0;
      if (lift + height() + s16(kReach) < static_cast<std::int16_t>(place)) {
        dropOffEnd();
        return;
      }
    } else if (height() < s16(kPlace)) {
      setU16(kPlace, climbableU16(kBottomExtent) - climbableU16(kTopExtent));
    }
    if ((mode_ & 0x40u) != 0u || !onGround()) {
      return;
    }
    letGo();
    setU8(kPhase, 0);
    if (mode_ == kTurnMode) {
      const std::uint8_t side = u8(kSide);
      setU8(kSide, 1u - side);
      call(tomba_call::kStopFacing, tomba_);
      setU8(kMode, kTurnedAction);
      return;
    }
    stand();
  }

private:
  std::uint16_t climbableU16(std::uint32_t field) const {
    return core_.mem_r16(climbable_ + field);
  }
  std::int32_t climbableS16(std::uint32_t field) const {
    return static_cast<std::int16_t>(climbableU16(field));
  }
  std::int32_t height() const {
    return climbableS16(kBottomExtent) - climbableS16(kTopExtent);
  }
  bool movingClimbable() const {
    return core_.mem_r8(climbable_ + 2u) == kMovingClimbable && core_.mem_r8(climbable_ + 0x47u) != kStillState;
  }
  bool onGround() {
    return (result(tomba_call::kGroundCheck, tomba_) & 1u) != 0u;
  }

  std::uint16_t stepSize() {
    if ((mode_ & 4u) != 0u) {
      const auto speed = static_cast<std::uint16_t>(u16(kVelocityY) + 4u);
      setU16(kVelocityY, speed);
      if (static_cast<std::int16_t>(speed) >= 0x1E) {
        setU16(kVelocityY, 0x1Eu);
      }
      return u16(kVelocityY);
    }
    const std::uint32_t index = (mode_ & 1u) + (u8(kFastClimb) != 0u ? 3u : 0u);
    return static_cast<std::uint16_t>(static_cast<std::int8_t>(core_.mem_r8(kStepTable + index)));
  }

  void up(std::uint16_t step) {
    const std::uint16_t place = u16(kPlace);
    setU16(kFloorAttribute, 0);
    const std::int16_t held = s16(kPlace);
    const std::uint32_t carried = static_cast<std::uint32_t>(climbableS16(kVelocityY) >> 8);
    if (movingClimbable()) {
      setU16(kPlace, (mode_ & 2u) != 0u ? place - 0x30u - carried : place + (step - carried));
    } else {
      setU16(kPlace, u16(kPlace) + step);
    }
    if (result(tomba_call::kFloorStep, tomba_, 1u) != 0u) {
      setU16(kPlace, static_cast<std::uint16_t>(held));
    }
    if (s16(kPlace) < -climbableS16(kTopExtent)) {
      setU16(kPlace, 0u - climbableU16(kTopExtent));
    }
  }

  void dropOffEnd() {
    setU16(kVelocityY, 0);
    setU16(kDropVelocity, 0);
    setU8(kGrip, 0);
    core_.mem_w8(climbable_ + 0x29u, 0);
    setU8(kClimbFlags, 0);
    core_.mem_w32(tomba_ + kClimbTimer, 0);
    setU16(kSpeedX, 0);
    setU16(kPlace, u16(kPlace) + kEndDrop);
    call(tomba_call::kStartFall, tomba_, 0u);
    if (core_.mem_r8(climbable_ + 2u) != kMovingClimbable) {
      return;
    }
    setU16(kVelocityY, climbableU16(kVelocityY));
    const auto place = static_cast<std::uint16_t>(u16(kPlace) + kEndDrop);
    setU16(kPlace, place);
    if (has(SuitAbility::AirControl)) {
      setU16(kPlace, place + ((climbableU16(kVelocityY) & 0x1F00u) >> 5u));
    }
  }

  void letGo() {
    core_.mem_w8(climbable_ + 0x29u, 0);
    setU8(kClimbFlags, 0);
    core_.mem_w32(tomba_ + kClimbTimer, 0);
    setU8(kSubstate, 0);
  }

  void stand() {
    setU16(kVelocityY, 0);
    setU16(kDropVelocity, 0);
    setU8(kGrip, 0);
    setU8(kMode, 0);
    call(tomba_call::kSetAnimation, tomba_, 2u, 3u);
  }

  std::uint32_t climbable_;
  std::uint32_t mode_;
};

} // namespace

void ClimbStep::step(Core *core) {
  psx::cpu::PreservedReturnAddress returnAddress(*core);
  psx::cpu::GuestFrameScope frame(*core, core->r[psx::cpu::kSp] - kStepFrameSize);
  Step(*core).run();
}

void ClimbStep::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declare(kStep, "ClimbStep::step", &ClimbStep::step, psx::cpu::NativeReturn::Void);
}

} // namespace tomba
