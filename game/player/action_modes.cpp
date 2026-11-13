#include "player/action_modes.h"

#include "guest_abi.h"
#include "overrides/native_override_catalog.h"
#include "player/tomba_step.h"

#include <cstdint>

namespace tomba {
namespace {

using namespace tomba_field;
using namespace tomba_call;

constexpr std::uint32_t kMode3 = 0x8005B63Cu;
constexpr std::uint32_t kMode6 = 0x8005D16Cu;
constexpr std::uint32_t kMode16 = 0x8005C26Cu;
constexpr std::uint32_t kFrameSize = 0x20u;

constexpr std::uint32_t kFun80053670 = 0x80053670u;
constexpr std::uint32_t kFun800532A0 = 0x800532A0u;
constexpr std::uint32_t kFun8005376C = 0x8005376Cu;
constexpr std::uint32_t kFun800537B8 = 0x800537B8u;
constexpr std::uint32_t kFun80053BF8 = 0x80053BF8u;
constexpr std::uint32_t kFun80053D0C = 0x80053D0Cu;
constexpr std::uint32_t kSettleStep = 0x80054650u; // ActorTomba_settleStep
constexpr std::uint32_t kFun8005490C = 0x8005490Cu;
constexpr std::uint32_t kFun800551C4 = 0x800551C4u;
constexpr std::uint32_t kFun80055284 = 0x80055284u;
constexpr std::uint32_t kFun80055704 = 0x80055704u;
constexpr std::uint32_t kFun80055D5C = 0x80055D5Cu;
constexpr std::uint32_t kFun80055E28 = 0x80055E28u;
constexpr std::uint32_t kFun80055FBC = 0x80055FBCu;
constexpr std::uint32_t kFun80057C08 = 0x80057C08u;
constexpr std::uint32_t kFun8005B134 = 0x8005B134u;
constexpr std::uint32_t kFun8005B20C = 0x8005B20Cu;
constexpr std::uint32_t kFun8005B370 = 0x8005B370u;
constexpr std::uint32_t kTargetKind = 0x8005B5E4u; // (target) -> its kind, 0 none
constexpr std::uint32_t kFun8005C138 = 0x8005C138u;
constexpr std::uint32_t kFun80113628 = 0x80113628u; // an area overlay's hold 9

// Each water area's hold step (Tomba, 0), by area 0, 6, 8, 14.
constexpr std::uint32_t kAreaHold0 = 0x8010C89Cu;
constexpr std::uint32_t kAreaHold6 = 0x8011460Cu;
constexpr std::uint32_t kAreaHold8 = 0x801120C4u;
constexpr std::uint32_t kAreaHold14 = 0x8010B408u;

constexpr std::uint32_t kArea = 0x800BF870u;
constexpr std::uint32_t kRejump = 0x800BF848u;
constexpr std::uint32_t kCircleJumpHeld = 0x1F800252u;
constexpr std::uint32_t kFloorAttributeOut = 0x1F8001A6u;
constexpr std::uint32_t kSpeedCurve = 0x800A4694u; // u16[11]

constexpr std::uint32_t kHoldSound = 0x20u;
constexpr std::uint32_t kThrowSound = 0x21u;
constexpr std::uint8_t kHoldPose = 0x26u;
constexpr std::uint8_t kFallPose = 0x15u;
constexpr std::uint8_t kJumpPose = 0x14u;
constexpr std::uint32_t kAirMode = 2u;
constexpr std::uint32_t kGlideArmed = 0xCu;
constexpr std::uint32_t kExtra = 0x181u;

class Mode : public TombaStep {
public:
  Mode(Core &core, const char *owner) : TombaStep(core, owner), target_(core.mem_r32(core.r[4] + kTarget)) {}

protected:
  // The grounded motion every hold substate runs first.
  void groundMotion(std::uint32_t edge, std::uint32_t floor) {
    call(kFun80055E28, tomba_, edge);
    call(kFun80055FBC, tomba_, floor);
    call(tomba_call::kVelocityIntegrate, tomba_, 1u);
    call(kFun80055D5C, tomba_);
  }
  std::uint32_t animate() {
    return result(kAnimationStep, tomba_);
  }
  bool jumpPressed() {
    return result(kJumpPressed) != 0u;
  }
  std::uint8_t targetU8(std::uint32_t field) const {
    return core_.mem_r8(target_ + field);
  }
  void setTargetU8(std::uint32_t field, std::uint32_t value) {
    core_.mem_w8(target_ + field, static_cast<std::uint8_t>(value));
  }
  std::int32_t targetKind() {
    return static_cast<std::int32_t>(result(kTargetKind, target_));
  }
  void setPose(std::uint32_t pose, std::uint32_t blend) {
    call(kSetAnimation, tomba_, pose, blend);
  }
  void nextPose() {
    setPose(u8(kAnimation) + 1u, 0u);
  }
  // After the airborne step: with the glide the apex latch stays clear.
  void airborne(std::uint32_t mode) {
    call(kAirborneStep, tomba_, mode);
  }
  void glideKeepsApexClear() {
    if (has(SuitAbility::Glide)) {
      setU8(kApexLatch, 0);
    }
  }
  void leaveToFall() {
    setU8(kMode, kAirMode);
    setU8(kSubstate, 1);
  }
  // The released target flies: Tomba is airborne again with the hold pose.
  void releasePose() {
    setU8(kAirState, 1);
    setU8(kLanded, 0);
    setU8(kApexLatch, 0);
    setU8(0x148u, 0);
    setU16(kVelocityY, 0);
  }
  void speedCurveStart() {
    setU16(0x40u, 0);
    setU16(0x42u, 0);
    setU16(kGroundSpeed, core_.mem_r16(kSpeedCurve));
    stepSubstate();
  }
  void speedCurveStep() {
    setU16(0x40u, u16(0x40u) + 1u);
    if (s16(0x40u) > 10) {
      setU16(0x40u, 10);
    }
    setU16(kGroundSpeed, core_.mem_r16(kSpeedCurve + static_cast<std::uint32_t>(s16(0x40u)) * 2u));
  }
  // Ground speed ramps to 0x1000, then the fall pose takes over.
  void rampToFall() {
    setU16(kGroundSpeed, u16(kGroundSpeed) + 0x200u);
    if (s16(kGroundSpeed) >= 0x1000) {
      setU16(kGroundSpeed, 0x1000);
      leaveToFall();
      setPose(kFallPose, 3u);
    }
  }
  bool landed() const {
    return u8(kLanded) != 0u;
  }
  std::uint16_t frameOfAnimation() const {
    return core_.mem_r16(core_.mem_r32(tomba_ + kAnimationData) + 2u);
  }
  // The floor under Tomba after a placed hold.
  void probeFloor() {
    setU16(kFloorAttribute, 0);
    if (result(kFloorProbe, tomba_, 0u, static_cast<std::int32_t>(s16(kLean))) != 0u) {
      setU16(kFloorAttribute, core_.mem_r16(kFloorAttributeOut));
    }
  }

  // The throw: the target lets go on releaseFrame; a hold under dropBelow drops it (state 1), else it flies (3).
  struct Throw {
    std::uint16_t releaseFrame;
    std::uint8_t dropBelow;
  };
  void throwTarget(Throw how) {
    speedCurveStep();
    if (s16(0x42u) == 0) {
      call(kFun8005B20C, tomba_, 1u);
      if (frameOfAnimation() == how.releaseFrame) {
        call(kSfxTrigger, kThrowSound, 0u, 0u);
        if (u8(kHold) < how.dropBelow) {
          setTargetU8(5u, 1u);
        } else {
          setTargetU8(5u, 3u);
          setTargetU8(6u, 0u);
        }
        setU16(0x42u, 1);
        setU8(kHold, 0);
      }
    }
    if (animate() == 1u) {
      nextPose();
      releasePose();
      setU8(kPhase, 1);
      stepSubstate();
    }
  }

  std::uint32_t target_;
};

class Mode3 : public Mode {
public:
  explicit Mode3(Core &core) : Mode(core, "ActionModes::mode3") {}

  void run() {
    switch (u8(kSubstate)) {
    case 0:
      if (!enter()) {
        return;
      }
      [[fallthrough]];
    case 1:
      approach();
      break;
    case 2:
      settle();
      return;
    case 3:
      call(kSfxTrigger, kHoldSound, 0u, 0u);
      setU16(kGroundSpeed, 0);
      nextPose();
      setU8(kPhase, 0);
      stepSubstate();
      [[fallthrough]];
    case 4:
      jumpWithTarget();
      call(kFun8005B20C, tomba_, 0u);
      return;
    case 5:
      call(tomba_call::kFloorStep, tomba_, 0u);
      setPose(throwFrames() == 6u ? 0x24u : 0x22u, 0u);
      speedCurveStart();
      call(kFun8005B20C, tomba_, 1u);
      return;
    case 6:
      throwTarget({.releaseFrame = throwFrames(), .dropBelow = 5u});
      break;
    case 7:
      groundMotion(0u, u8(kFloor));
      animate();
      if (frameOfAnimation() >= 2u) {
        rampToFall();
      }
      airborne(8u);
      call(kFun80057C08, tomba_, 0u);
      return;
    case 8:
      if (animate() == 1u || targetU8(2u) == 0x1Bu) {
        if (result(kFun80053BF8, tomba_, 0u) != 0u) {
          call(kFun80055D5C, tomba_);
          call(kFun80053BF8, tomba_, 1u);
        }
        setU8(kApexLatch, 0);
        stepSubstate();
      }
      call(kFun80055E28, tomba_, 1u);
      placeHold(targetKind());
      return;
    case 9:
      placeAgain();
      return;
    case 10:
      if (!leap()) {
        return;
      }
      [[fallthrough]];
    case 0xB:
      groundMotion(1u, u8(kFloor));
      airborne(1u);
      if (u8(kAirState) == 2u) {
        leaveToFall();
        setPose(kFallPose, 3u);
      }
      break;
    case 0xC:
      groundMotion(1u, u8(kFloor));
      airborne(1u);
      if (u8(kAirState) == 2u) {
        setPose(kHoldPose, 0u);
        setU8(kMode, 0x10u);
        setU8(kSubstate, 5);
        setU16(kGroundSpeed, 0x20u);
      }
      break;
    default:
      return;
    }
    call(kFun800551C4, tomba_);
  }

private:
  // 0x6 with the floor bit 8 and a hold under 6, else 5: the animation frame the throw lets go at.
  std::uint16_t throwFrames() const {
    return (u8(kFloor) & 8u) != 0u && u8(kHold) < 6u ? 6u : 5u;
  }

  // Substate 0: false when it already finished this frame.
  bool enter() {
    setU8(kSide, u8(kFloor) & 1u);
    call(kFun80055284, tomba_);
    setU8(0x16Au, 0);
    call(kFun80053D0C, tomba_);
    call(kFun80053BF8, tomba_, 0u);
    setU8(0x146u, 0);
    setU16(kVelocityY, 0);
    const std::int32_t kind = targetKind();
    if (kind != 0) {
      setU8(kSubstate, 8);
      setU8(kAirState, 0);
      setU16(0x44u, 0);
      call(kFun8005B370, tomba_, kind < 3 ? 0u : 2u);
      setU16(kVelocityY, 0);
      call(kFun800551C4, tomba_);
      return false;
    }
    if (u8(kExtra) != 0u) {
      setU8(kExtra, 1);
      setU8(kSubstate, 2);
      setU8(kApexLatch, 0);
      return false;
    }
    stepSubstate();
    return true;
  }

  void approach() {
    groundMotion(1u, u8(kFloor));
    if (u8(kExtra) == 0u) {
      airborne(6u);
      glideKeepsApexClear();
    }
    if (result(kFun80053BF8, tomba_, 0u) == 0u) {
      animate();
      if (jumpPressed()) {
        setU8(kSubstate, 3);
        setU8(kApexLatch, 0);
        setU8(kExtra, 0);
        setU8(kAirState, 1);
        setU16(kVelocityY, 0);
      } else if (landed() || u8(kExtra) != 0u) {
        if (landed()) {
          setU16(kVelocityY, 0);
        }
        setU8(kApexLatch, 0);
        stepSubstate();
      }
    } else {
      animate();
      if (landed() || u8(kExtra) != 0u) {
        if (landed()) {
          setU16(kVelocityY, 0);
        }
        setU8(kApexLatch, 0);
        stepSubstate();
        call(kFun80053BF8, tomba_, 1u);
      }
    }
    call(kFun8005B20C, tomba_, 0u);
  }

  void settle() {
    if (u8(kExtra) == 0u) {
      call(kFun80055E28, tomba_, 1u);
      call(kFun80055FBC, tomba_, u8(kFloor) | 2u);
      call(tomba_call::kVelocityIntegrate, tomba_, 1u);
      call(kFun80055D5C, tomba_);
      setU16(kY + 2u, u16(kY + 2u) + 8u);
      call(kGroundCheck, tomba_);
      if (u8(kHold) == 6u) {
        setU8(kSide, 0);
      }
      call(kFun800551C4, tomba_);
      call(kDropStep, tomba_, 1u);
      animate();
      if (jumpPressed()) {
        setU8(kExtra, 0);
        setU8(kAirState, 1);
        setU16(kVelocityY, 0);
        stepSubstate();
        return;
      }
    } else {
      setU8(kExtra, 1);
      const std::int32_t sway = s16(0x44u);
      const std::int32_t magnitude = sway < 0 ? -sway : sway;
      setU8(kApexLatch, 0);
      if ((magnitude & 0xFFFF) != 0) {
        std::int32_t eased = magnitude - 0x120;
        if (static_cast<std::int16_t>(eased) < 0) {
          eased = 0;
        }
        setU16(0x44u, static_cast<std::uint32_t>(sway <= 0 ? -eased : eased));
      }
      call(kFun80055E28, tomba_, 1u);
      const std::uint8_t area = core_.mem_r8(kArea);
      if (area == 0u) {
        call(kAreaHold0, tomba_, 0u);
      } else if (area == 6u) {
        call(kAreaHold6, tomba_, 0u);
      } else if (area == 8u) {
        call(kAreaHold8, tomba_, 0u);
      } else if (area == 0xEu) {
        call(kAreaHold14, tomba_, 0u);
      }
      animate();
      if (jumpPressed()) {
        setU8(kAirState, 1);
        setU8(kExtra, 0);
        stepSubstate();
        return;
      }
    }
    call(kFun8005B20C, tomba_, 0u);
  }

  void jumpWithTarget() {
    groundMotion(1u, u8(kFloor));
    airborne(2u);
    if (core_.mem_r8(kRejump) == 0u && (jumpPressed() || landed())) {
      stepSubstate();
      const std::uint8_t hold = u8(kHold);
      if (hold == 9u) {
        call(kFun80113628, tomba_, target_);
      } else if (hold < 5u) {
        setU8(kHold, 0);
        if (targetU8(2u) == 0x10u) {
          setTargetU8(6u, targetU8(6u) + 1u);
        } else {
          setTargetU8(4u, 3u);
        }
        const auto placed = static_cast<std::int32_t>(result(kFun8005B134, tomba_, target_));
        const std::uint32_t next = placed == -1 ? 0u : result(kFun800537B8, tomba_, target_);
        if (next == 0u) {
          setPose(kHoldPose, 0u);
          setU8(kAirState, 1);
          setU8(kPhase, 1);
          setU8(kSubstate, 7);
          setU8(kLanded, 0);
          setU8(kApexLatch, 0);
          setU8(0x148u, 0);
          setU16(kVelocityY, 0);
          setU16(kGroundSpeed, 0x20u);
        } else {
          setU8(kHold, 3);
          core_.mem_w32(tomba_ + kTarget, next);
        }
      }
    }
    call(kFun800551C4, tomba_);
  }

  // Substates 8 and 9 end here: the hold placed on the target, Tomba standing on it.
  void placeHold(std::int32_t kind) {
    call(kFun8005B370, tomba_, kind < 3 ? 0u : 2u);
    call(kFun800551C4, tomba_);
    setU8(kLanded, 1);
    probeFloor();
  }

  void placeAgain() {
    animate();
    call(kFun80055E28, tomba_, 1u);
    if (s16(0x16Eu) == s16(0x170u) && jumpPressed()) {
      setU8(kFloor, u8(kSide) + 2u);
      const std::int32_t kind = targetKind();
      if (kind == 2 || kind == 4) {
        if (kind == 4) {
          setU8(kHold, 5);
        } else if (u8(kHold) != 9u) {
          setU8(kHold, 1);
        }
        setTargetU8(6u, targetU8(6u) + 1u);
        setU8(kSubstate, 3);
      } else {
        stepSubstate();
      }
    }
    placeHold(targetKind());
  }

  // Substate 10: false when it already finished this frame.
  bool leap() {
    call(kSfxTrigger, kHoldSound, 0u, 0u);
    nextPose();
    setU8(kPhase, 0);
    setU16(kGroundSpeed, 0);
    stepSubstate();
    setTargetU8(5u, 2u);
    setTargetU8(6u, 0u);
    setU8(kHold, 0);
    if (targetKind() == 3) {
      setU8(kSubstate, 0xC);
      setU16(kVelocityY, 0xCD00u);
      return false;
    }
    setTargetU8(0u, targetU8(2u) == 0x13u ? 0xBu : 3u);
    setU16(kVelocityY, 0xC500u);
    return true;
  }
};

class Mode16 : public Mode {
public:
  explicit Mode16(Core &core) : Mode(core, "ActionModes::mode16") {}

  void run() {
    switch (u8(kSubstate)) {
    case 0:
      if (!enter()) {
        return;
      }
      [[fallthrough]];
    case 1:
      approach();
      break;
    case 2:
      settle();
      return;
    case 3:
      call(kSfxTrigger, kHoldSound, 0u, 0u);
      setU16(kGroundSpeed, 0);
      nextPose();
      setU8(kPhase, 0);
      stepSubstate();
      [[fallthrough]];
    case 4:
      jumpWithTarget();
      break;
    case 5:
      call(kFun80055D5C, tomba_);
      animate();
      rampToFall();
      airborne(8u);
      call(kFun80057C08, tomba_, 0u);
      return;
    case 6:
      setPose(0x24u, 0u);
      speedCurveStart();
      call(kFun8005B20C, tomba_, 1u);
      return;
    case 7:
      throwTarget({.releaseFrame = 6u, .dropBelow = 0u});
      break;
    case 8:
      call(kFun80055D5C, tomba_);
      animate();
      if (frameOfAnimation() >= 2u) {
        rampToFall();
      }
      airborne(8u);
      call(kFun80057C08, tomba_, 0u);
      return;
    case 9:
      call(kFun80055D5C, tomba_);
      animate();
      setU16(kGroundSpeed, u16(kGroundSpeed) + 0x200u);
      if (s16(kGroundSpeed) >= 0x1000) {
        setU8(kMode, 3);
        setU16(kGroundSpeed, 0);
        setU8(kSubstate, 7);
      }
      return;
    default:
      return;
    }
    call(kFun800551C4, tomba_);
  }

private:
  bool enter() {
    setU8(kSide, u8(kFloor) & 1u);
    call(kFun80055284, tomba_);
    setU8(0x16Au, 0);
    call(kFun80053D0C, tomba_);
    call(kFun80053BF8, tomba_, 0u);
    setU8(0x146u, 0);
    setU16(0x44u, 0);
    call(tomba_call::kStopFacing, tomba_);
    setU16(kVelocityY, 0);
    const std::uint8_t hold = u8(kHold);
    if (hold == 4u || hold == 7u) {
      call(kFun8005B370, tomba_, 0u);
      call(kFun800551C4, tomba_);
      setU8(kSubstate, 2);
      setU8(kApexLatch, 0);
      setU16(kVelocityY, 0);
      return false;
    }
    setU8(kSubstate, 1);
    return true;
  }

  void approach() {
    call(kFun80055D5C, tomba_);
    airborne(6u);
    glideKeepsApexClear();
    if (result(kFun80053BF8, tomba_, 0u) == 0u) {
      animate();
      if (jumpPressed()) {
        setU8(kSubstate, 3);
        setU8(kApexLatch, 0);
        setU16(kVelocityY, 0);
        setU8(kAirState, 1);
      } else if (landed()) {
        setU16(kVelocityY, 0);
        setU8(kApexLatch, 0);
        stepSubstate();
      }
    } else {
      animate();
      if (landed()) {
        setU16(kVelocityY, 0);
        setU8(kApexLatch, 0);
        stepSubstate();
        call(kFun80053BF8, tomba_, 1u);
      }
    }
    call(kFun8005B370, tomba_, 1u);
  }

  void settle() {
    animate();
    if (u8(kHold) == 7u) {
      call(kFun8005B370, tomba_, 0u);
      call(kFun800551C4, tomba_);
      return;
    }
    call(kFun80055E28, tomba_, 1u);
    call(kFun80055D5C, tomba_);
    if (u8(kHold) == 4u) {
      call(kFun8005B370, tomba_, 0u);
      call(kFun800551C4, tomba_);
      probeFloor();
    } else {
      setU16(kY + 2u, u16(kY + 2u) + 8u);
      call(kGroundCheck, tomba_);
      call(kFun8005B370, tomba_, 1u);
      call(kFun800551C4, tomba_);
      call(kDropStep, tomba_, 1u);
    }
    if (jumpPressed()) {
      setU16(kVelocityY, 0);
      setU8(kAirState, 1);
      stepSubstate();
    }
  }

  void jumpWithTarget() {
    if (u8(kHold) == 8u) {
      call(kFun80055E28, tomba_, 1u);
      call(kFun80055FBC, tomba_, u8(kFloor));
      call(tomba_call::kVelocityIntegrate, tomba_, 1u);
    }
    call(kFun80055D5C, tomba_);
    airborne(2u);
    if (u8(kHold) == 8u) {
      call(kFun8005B20C, tomba_, 0u);
    } else {
      call(kFun8005B370, tomba_, 1u);
    }
    if (core_.mem_r8(kRejump) != 0u || !(jumpPressed() || landed())) {
      return;
    }
    if (u8(kHold) == 4u) {
      setU8(kSubstate, 6);
      return;
    }
    if (result(kFun8005C138, tomba_, target_) == 0u) {
      setTargetU8(4u, 3u);
    }
    setU8(kSubstate, u8(kHold) == 8u ? 9u : u8(kSubstate) + 1u);
    setU8(kPhase, 1);
    setU8(kHold, 0);
    setPose(kHoldPose, 0u);
    releasePose();
    setU16(kGroundSpeed, 0x20u);
  }
};

class Mode6 : public Mode {
public:
  explicit Mode6(Core &core) : Mode(core, "ActionModes::mode6") {}

  void run() {
    call(kFun80055E28, tomba_, 0u);
    if (carrying() && u8(kSubstate) != 2u) {
      if (u8(0x16Bu) == 0u && u8(0x61u) == 0u) {
        call(kSettleStep, tomba_, 0u);
      } else {
        setU8(0x5Fu, u8(0x5Fu) & 0xFBu);
      }
    } else {
      call(kFun80055FBC, tomba_, u8(kFloor));
      call(tomba_call::kVelocityIntegrate, tomba_, 1u);
    }
    call(kFun80055D5C, tomba_);
    switch (u8(kSubstate)) {
    case 0:
      takeOff();
      return;
    case 1:
      rise();
      return;
    case 2:
      fall();
      return;
    default:
      return;
    }
  }

private:
  // G+0x6C 1 or 2: Tomba carries something.
  bool carrying() const {
    return static_cast<std::uint8_t>(u8(0x6Cu) - 1u) < 2u;
  }

  void takeOff() {
    setU8(kExtra, 0);
    core_.mem_w8(kCircleJumpHeld, 0);
    setU16(kGroundSpeed, 0);
    setU8(0xBFu, 0);
    setU8(0xBEu, 0);
    setU8(kSide, u8(kFloor) & 1u);
    call(kFun800551C4, tomba_);
    if (u8(0x175u) == 0u) {
      setU8(0x146u, 1);
      call(kFun80053670, tomba_, 0u, 0u);
      call(kFun8005490C, tomba_, 0u, 0u);
      stepSubstate();
    } else {
      setU8(0x146u, 2);
      call(kFun80053670, tomba_, 1u, 0u);
      call(kFun8005490C, tomba_, 1u, 1u);
      setU8(kSubstate, 2);
    }
    if (!carrying()) {
      airborne(0u);
    }
  }

  void rise() {
    const std::uint32_t lastFrame = u8(0x79u) != 2u ? animate() : 0u;
    if (!carrying()) {
      airborne(0u);
    } else if (core_.mem_r8(kCircleJumpHeld) != 0u) {
      if (has(SuitAbility::Glide)) {
        setU8(kAirFrames, kGlideArmed);
      }
      airborne(0u);
    }
    if (landed()) {
      if (result(kFun800532A0, tomba_) != 0u) {
        setU8(0x146u, 0);
        return;
      }
      setU8(kMode, 5);
      setU8(0x149u, (u8(0x149u) & 1u) | 2u);
      setU8(kPhase, 0);
      setU8(kFloor, (u8(kFloor) & 0xEu) | u8(kSide));
      return;
    }
    if (lastFrame == 0u || result(kFun8005376C, tomba_) != 0u) {
      return;
    }
    if (static_cast<std::int32_t>(result(kFun80055704, tomba_, 0u)) == 2) {
      setU8(0x146u, 2);
      call(kFun80053670, tomba_, 1u, 0u);
      stepSubstate();
      return;
    }
    leaveToFall();
    setU8(0x146u, 0);
    setPose(kJumpPose, 2u);
  }

  void fall() {
    setU8(kExtra, 0);
    animate();
    airborne(0u);
    setU8(kSide, u8(kFloor) & 1u);
    call(kFun8005490C, tomba_, 1u, 1u);
    call(kFun800551C4, tomba_);
    if (landed()) {
      if (result(kFun800532A0, tomba_) != 0u) {
        setU8(0x146u, 0);
        return;
      }
      setU8(kMode, 5);
      setU8(kPhase, 0);
      setU8(0x149u, (u8(0x149u) & 1u) | 2u);
      return;
    }
    const auto next = static_cast<std::int32_t>(result(kFun80055704, tomba_, 1u));
    if (next == -1) {
      leaveToFall();
      setU8(0x146u, 0);
      setPose(kJumpPose, 2u);
      return;
    }
    if (next != 0) {
      return;
    }
    core_.mem_w8(kCircleJumpHeld, 0);
    setU8(0x146u, 3);
    call(kFun80053670, tomba_, 0u, 8u);
    call(kFun8005490C, tomba_, 0u, 0u);
    setU8(0xBEu, 0);
    setU8(kSubstate, 1);
  }
};

template <typename M> void runMode(Core *core) {
  psx::cpu::PreservedReturnAddress returnAddress(*core);
  psx::cpu::GuestFrameScope frame(*core, core->r[psx::cpu::kSp] - kFrameSize);
  M(*core).run();
}

} // namespace

void ActionModes::mode3(Core *core) {
  runMode<Mode3>(core);
}

void ActionModes::mode6(Core *core) {
  runMode<Mode6>(core);
}

void ActionModes::mode16(Core *core) {
  runMode<Mode16>(core);
}

void ActionModes::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declare(kMode3, "ActionModes::mode3", &ActionModes::mode3);
  natives.declare(kMode6, "ActionModes::mode6", &ActionModes::mode6);
  natives.declare(kMode16, "ActionModes::mode16", &ActionModes::mode16);
}

} // namespace tomba
