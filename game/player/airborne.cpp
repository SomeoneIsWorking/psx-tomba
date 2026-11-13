#include "player/airborne.h"

#include "core.h"
#include "guest_abi.h"
#include "guest_call.h"
#include "overrides/native_override_catalog.h"
#include "player/tomba_step.h"

namespace tomba {
namespace {

using namespace tomba_field;
using tomba_call::kGroundCheck;
using tomba_call::kSfxTrigger;
constexpr std::uint32_t kWalkStart = tomba_call::kSetAnimation;

constexpr std::uint32_t kStep = tomba_call::kAirborneStep;
constexpr std::uint32_t kFrameSize = 0x30u;
constexpr std::uint32_t kDropFrameSize = 0x18u;
constexpr std::uint32_t kLandingTarget = 0x10u; // sp offset of the landing position vector

constexpr std::uint32_t kFun8005749C = 0x8005749Cu;
constexpr std::uint32_t kLaunch = 0x80055F48u;
constexpr std::uint32_t kAirActions = 0x80057C08u;
constexpr std::uint32_t kFun80057A68 = 0x80057A68u;
constexpr std::uint32_t kFun80055634 = 0x80055634u; // (Tomba, 0): nonzero on the attack button
constexpr std::uint32_t kFun800532A0 = 0x800532A0u; // (Tomba): nonzero keeps him off the landing
constexpr std::uint32_t kFun80054E80 = 0x80054E80u;
constexpr std::uint8_t kStompMode = 0x1Cu;
constexpr std::uint32_t kFun800543C0 = 0x800543C0u;
constexpr std::uint32_t kFun80056F3C = 0x80056F3Cu;
constexpr std::uint32_t kFun80056EC8 = 0x80056EC8u;
constexpr std::uint32_t kFun800538E0 = 0x800538E0u;
constexpr std::uint32_t kFun8005314C = 0x8005314Cu; // the landing impact
constexpr std::uint32_t kApproach = 0x800776F8u;    // (target, value, step) -> value moved toward target

// 0x800BF848: nonzero, a held jump restarts the take-off in the air (bits 0..1 during the rise).
constexpr std::uint32_t kRejump = 0x800BF848u;
constexpr std::uint32_t kGameOverLatch = 0x800BF80Du;
constexpr std::uint32_t kPadHeld = 0x800E7E68u;
constexpr std::uint32_t kPadHeldNow = 0x800ECF54u;
constexpr std::uint32_t kLaunchSpeeds = 0x800A457Cu;    // u16 by G+0x6D
constexpr std::uint32_t kBoostedLaunch = 0x800A4582u;   // with G+0x174 & 0x30
constexpr std::uint32_t kReleaseBrake = 0x800A4588u;    // u16[11] by frames held
constexpr std::uint32_t kPigReleaseBrake = 0x800A45A0u; // u16[11] with the Pig suits
constexpr std::uint16_t kPigLaunchDamping = 0x1940u;
constexpr std::uint32_t kJumpHeldFrames = 0x167u;
constexpr std::uint32_t kJumpButton = 0x1F800172u;

constexpr std::uint8_t kJumpPose = 0x14u;
constexpr std::uint8_t kGlideReady = 0xCAu;
constexpr std::uint8_t kGliding = 0xCBu;
constexpr std::uint8_t kFastFallPose = 0xE5u;
constexpr std::uint32_t kJumpSound = 0x1Du;
constexpr std::uint32_t kGlideSound = 0x35u;

constexpr std::uint16_t kGravity = 0x360u;
constexpr std::uint16_t kPigRiseGravity = 0x120u;
constexpr std::uint16_t kFastFallGravity = 0x500u;
constexpr std::int16_t kTerminalVelocity = 0x3E00;
constexpr std::uint32_t kGlideCap = 0x300u;
constexpr std::uint32_t kFallCap = 0xE00u;
constexpr std::uint32_t kSlopeCap = 0x1A00u;
constexpr std::uint32_t kLandingMode = 2u;
constexpr std::uint32_t kStillMode = 8u;

constexpr std::uint16_t kDropGravity = 0x800u;
constexpr std::int16_t kDropFallStart = 0x1E00;
constexpr std::int16_t kHardLanding = 0x1400;

constexpr std::uint32_t kFacingX = 0x48u;
constexpr std::uint32_t kFacingZ = 0x4Cu;

class Step : public TombaStep {
public:
  // The guest call's arguments: a0 Tomba, a1 the mode.
  explicit Step(Core &core) : TombaStep(core, "Airborne::step"), mode_(core.r[5]) {}

  void run();

private:
  bool jumpHeld() const {
    return (core_.mem_r16(kPadHeld) & core_.mem_r16(kJumpButton)) != 0u;
  }
  bool airControl() const {
    return (mode_ & 0x10u) == 0u && core_.mem_r8(kRejump) == 0u && u8(0x61u) == 0u;
  }
  // Velocity += gravity as the u16 the guest keeps; returns the new value.
  std::int16_t accelerate(std::uint16_t gravity) {
    const auto velocity = static_cast<std::uint16_t>(u16(kVelocityY) + gravity);
    setU16(kVelocityY, velocity);
    return static_cast<std::int16_t>(velocity);
  }
  void moveByVelocity() {
    const auto offset = static_cast<std::uint32_t>(static_cast<std::int32_t>(s16(kVelocityY)) * 0x100);
    core_.mem_w32(tomba_ + kY, core_.mem_r32(tomba_ + kY) + offset);
  }
  void approachGroundSpeed(std::uint32_t target, std::uint32_t rate) {
    const std::uint32_t speed =
        result(kApproach, target, static_cast<std::uint32_t>(static_cast<std::int32_t>(s16(kGroundSpeed))), rate);
    setU16(kGroundSpeed, speed);
  }
  std::uint32_t landingProbe() {
    const auto lean = static_cast<std::uint32_t>(static_cast<std::int32_t>(s16(kLean)));
    const std::int32_t slope = static_cast<std::int32_t>(lean << 17) >> 16;
    const std::uint32_t landed = result(tomba_call::kFloorProbe, tomba_, 0u, slope);
    setU8(kLanded, landed);
    return landed & 0xFFu;
  }
  // The fall poses the glide takes over from.
  bool fallPose() const {
    const std::uint8_t animation = u8(kAnimation);
    return animation == 0x14u || animation == 0x15u || animation == 0x62u || animation == 0x12u;
  }
  void glideReadyFromFallPose() {
    if (fallPose()) {
      call(kWalkStart, tomba_, kGlideReady, 6u);
      setU8(kAirFrames, 0);
    }
  }

  std::uint32_t mode_;

  void takeOff();
  void rise();
  void riseWithWings();
  void fall();
  std::uint32_t fallWithGlide();
  std::uint32_t fallCap();
  void jumpRelease();
  void restartTakeOff();
  void land();
};

void Step::run() {
  const std::uint8_t phase = u8(kPhase);
  if (phase == 0u) {
    takeOff();
    rise();
  } else if (phase == 1u) {
    rise();
  } else if (phase <= 3u) {
    fall();
  }
}

void Step::takeOff() {
  call(kFun8005749C, tomba_);
  if ((mode_ & 1u) != 0u) {
    return;
  }
  call(kLaunch, tomba_, 1u);
  if (u8(0x165u) != 0u) {
    accelerate(static_cast<std::uint16_t>(-0x580));
  }
  if (u8(0x78u) == 6u) {
    accelerate(static_cast<std::uint16_t>(-0x1000));
  }
}

void Step::rise() {
  call(tomba_call::kFloorStep, tomba_, 0u);
  call(kFun800543C0, tomba_);
  if (airControl() && has(SuitAbility::AirControl)) {
    riseWithWings();
  } else {
    const bool pig = (mode_ & 1u) == 0u && has(SuitAbility::PigJump);
    if (accelerate(pig ? kPigRiseGravity : kGravity) >= 0) {
      setU8(kAirState, 2);
      setU8(kPhase, u8(kPhase) + 1u);
      if ((mode_ & 2u) == 0u) {
        setU8(kApexLatch, 1);
      }
    }
    moveByVelocity();
  }
  if ((mode_ & 1u) == 0u) {
    jumpRelease();
  }
  if ((core_.mem_r8(kRejump) & 3u) == 0u) {
    return;
  }
  if (u8(kApexLatch) == 1u) {
    setU8(kApexLatch, 0);
  }
  if (static_cast<std::int32_t>(mode_) < 2 && jumpHeld()) {
    restartTakeOff();
    return;
  }
  setU8(kPhase, 2);
  setU8(kAirState, 2);
}

// FUN_8005706C: past the apex a winged suit turns a fall pose into the glide-ready pose.
void Step::riseWithWings() {
  const std::int16_t velocity = accelerate(kGravity);
  if (velocity < 0) {
    moveByVelocity();
    return;
  }
  if (has(SuitAbility::Glide)) {
    glideReadyFromFallPose();
  } else {
    setU8(kAirFrames, 0);
    call(kFun80056EC8, tomba_);
  }
  setU8(kAirState, 2);
  setU8(kPhase, u8(kPhase) + 1u);
  if ((mode_ & 2u) == 0u) {
    setU8(kApexLatch, 1);
  }
}

void Step::fall() {
  if (has(SuitAbility::Flutter)) {
    call(kFun80056F3C, tomba_);
  }
  if (airControl() && (has(SuitAbility::Glide) || u8(0x179u) == 1u)) {
    if (fallWithGlide() != 0u) {
      return;
    }
  } else {
    call(tomba_call::kFloorStep, tomba_, 0u);
    if (u8(kLanded) != 0u) {
      land();
      return;
    }
    if (has(SuitAbility::AirControl)) {
      if (mode_ != kStillMode) {
        approachGroundSpeed(0u, 0x40u);
      }
      if (has(SuitAbility::Flutter)) {
        if ((u8(kFloor) & 8u) != 0u) {
          setU8(kAirFrames, u8(kAirFrames) + 1u);
        }
        const auto frames = static_cast<std::uint8_t>(u8(kAirFrames) + 1u);
        setU8(kAirFrames, frames);
        if (frames == 0xFFu) {
          setU8(kAirFrames, 0xFFu);
        }
      }
    }
    const bool fast = (mode_ & 1u) == 0u && u8(kAnimation) == kFastFallPose;
    if (accelerate(fast ? kFastFallGravity : kGravity) > kTerminalVelocity) {
      setU16(kVelocityY, kTerminalVelocity);
    }
    moveByVelocity();
    if (mode_ == kLandingMode) {
      if (core_.mem_r8(kRejump) != 0u && jumpHeld()) {
        setU8(kPhase, 0);
        setU8(kAirState, 1);
        call(kSfxTrigger, kJumpSound, 0u, 0u);
        return;
      }
      if (landingProbe() != 0u) {
        return;
      }
    } else if (core_.mem_r8(kRejump) != 0u) {
      if (u8(kApexLatch) == 1u) {
        setU8(kApexLatch, 0);
      }
      if (static_cast<std::int32_t>(mode_) < 2 && jumpHeld()) {
        restartTakeOff();
        return;
      }
    } else if (s16(kVelocityY) >= 0 && (mode_ & 2u) == 0u && u8(kApexLatch) == 0u) {
      setU8(kApexLatch, 1);
    }
    call(kGroundCheck, tomba_);
  }
  if (u8(kLanded) != 0u) {
    land();
  }
}

// FUN_800572EC: the fall under fallCap; nonzero is the landing result in the landing mode.
std::uint32_t Step::fallWithGlide() {
  call(tomba_call::kFloorStep, tomba_, 0u);
  if (u8(kLanded) != 0u) {
    return 0;
  }
  std::uint32_t cap = fallCap();
  if (has(SuitAbility::Glide)) {
    glideReadyFromFallPose();
  } else if ((u8(kFloor) & 8u) != 0u) {
    setU8(kAirFrames, u8(kAirFrames) + 1u);
  }
  const auto frames = static_cast<std::uint8_t>(u8(kAirFrames) + 1u);
  setU8(kAirFrames, frames);
  if (frames < 10u) {
    cap = 0;
  } else if (frames == 0xFFu) {
    setU8(kAirFrames, 0xFFu);
  }
  const std::int16_t velocity = accelerate(kGravity);
  if (static_cast<std::int32_t>(cap << 16) < static_cast<std::int32_t>(static_cast<std::uint32_t>(velocity) << 16)) {
    setU16(kVelocityY, cap);
  }
  moveByVelocity();
  if (mode_ == kLandingMode) {
    if (const std::uint32_t landed = landingProbe(); landed != 0u) {
      return landed;
    }
  } else if (s16(kVelocityY) >= 0 && (mode_ & 2u) == 0u && u8(kApexLatch) == 0u) {
    setU8(kApexLatch, 1);
  }
  call(kGroundCheck, tomba_);
  return 0;
}

// FUN_80057150: the fall speed cap.
std::uint32_t Step::fallCap() {
  if (has(SuitAbility::Glide)) {
    const std::uint8_t animation = u8(kAnimation);
    bool gliding = false;
    if (animation == kGliding) {
      // The glide's animation flags its last frame.
      if ((core_.mem_r16(core_.mem_r32(tomba_ + kAnimationData) + 6u) & 0x8000u) != 0u) {
        call(kWalkStart, tomba_, kGlideReady, 4u);
      } else {
        gliding = true;
      }
    } else if (animation == kGlideReady) {
      if (jumpHeld()) {
        call(kWalkStart, tomba_, kGliding, 4u);
        call(kSfxTrigger, kGlideSound, 0u, 0u);
        gliding = true;
      }
    } else {
      return kFallCap;
    }
    if (gliding) {
      return kGlideCap;
    }
  }
  std::uint32_t mode = mode_;
  if (u8(0x146u) != 0u || u8(0x78u) != 0u) {
    mode = kStillMode;
  }
  const std::uint8_t floor = u8(kFloor);
  if ((floor & 2u) == 0u && (floor & 1u) == u8(0x147u)) {
    if (mode != kStillMode) {
      approachGroundSpeed(0x60u, 0x10u);
    }
    return kFallCap;
  }
  if ((floor & 8u) != 0u) {
    if (mode != kStillMode) {
      approachGroundSpeed(0xF00u, 0x20u);
    }
    return kSlopeCap;
  }
  if (mode != kStillMode) {
    approachGroundSpeed(0u, 0x40u);
  }
  return kFallCap;
}

// FUN_80056E08: a jump let go within 10 frames is braked by the frames it was held; the Pig suits brake less.
void Step::jumpRelease() {
  if ((core_.mem_r16(kPadHeldNow) & core_.mem_r16(kJumpButton)) != 0u) {
    const auto held = static_cast<std::uint8_t>(u8(kJumpHeldFrames) + 1u);
    setU8(kJumpHeldFrames, held == 0xFFu ? 0xFEu : held);
    return;
  }
  const std::uint8_t held = u8(kJumpHeldFrames);
  if (held >= 0xBu) {
    return;
  }
  const std::uint32_t brake = has(SuitAbility::PigJump) ? kPigReleaseBrake : kReleaseBrake;
  setU16(kVelocityY, u16(kVelocityY) + core_.mem_r16(brake + held * 2u));
  setU8(kJumpHeldFrames, 0x10u);
  if (s16(kVelocityY) > 0) {
    setU16(kVelocityY, 0);
  }
}

// A held jump while hovering starts a new take-off.
void Step::restartTakeOff() {
  setU8(kPhase, 0);
  setU8(kAirState, 1);
  call(kWalkStart, tomba_, kJumpPose, 4u);
  call(kSfxTrigger, kJumpSound, 0u, 0u);
}

// Placed a step ahead of the landing point along the facing, on the side 0x147 names.
void Step::land() {
  setU8(0x148u, 0);
  core_.mem_w16(tomba_ + 0x50u, 0);
  if (u8(kApexLatch) < 2u) {
    setU8(kApexLatch, 0);
  }
  setU8(kAirState, 0);
  const std::uint32_t target = core_.r[psx::cpu::kSp] + kLandingTarget;
  const auto ahead = [this](std::uint32_t facing) {
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(s16(facing)) * 0x2800);
  };
  const bool forward = u8(0x147u) != 0u;
  const std::uint32_t x = core_.mem_r32(tomba_ + kX);
  const std::uint32_t z = core_.mem_r32(tomba_ + kZ);
  core_.mem_w32(target, forward ? x + ahead(kFacingX) : x - ahead(kFacingX));
  core_.mem_w32(target + 4u, core_.mem_r32(tomba_ + kY));
  core_.mem_w32(target + 8u, forward ? z + ahead(kFacingZ) : z - ahead(kFacingZ));
  call(kFun800538E0, tomba_, target, 0u);
  call(kFun8005314C, tomba_);
}

// FUN_80056C00 (a0 Tomba, a1 nonzero keeps him grounded): unsupported and not airborne, he sinks at a growing drop
// speed; past kDropFallStart the drop becomes the airborne fall, its speed a quarter with the glide.
// FUN_80055F48 (a0 Tomba, a1 nonzero for a jump): the take-off speed; the Pig suits' jump starts slower.
class Launch : public TombaStep {
public:
  explicit Launch(Core &core) : TombaStep(core, "Airborne::launch"), jump_(core.r[5]) {}

  void run() {
    if ((u8(0x174u) & 0x30u) != 0u) {
      setU16(kVelocityY, core_.mem_r16(kBoostedLaunch));
    } else {
      setU16(kVelocityY, core_.mem_r16(kLaunchSpeeds + u8(0x6Du) * 2u));
    }
    if (jump_ != 0u && has(SuitAbility::PigJump)) {
      setU16(kVelocityY, u16(kVelocityY) + kPigLaunchDamping);
    }
  }

private:
  std::uint32_t jump_;
};

// FUN_80057C08 (a0 Tomba, a1 mode): the moves a jump can turn into, and the landing. With the Pig suits a jump press
// in the air is the stomp (mode 0x1C, pose 0xE5); circle is the air attack (mode 6, or 5 on the ground).
class AirActions : public TombaStep {
public:
  explicit AirActions(Core &core) : TombaStep(core, "Airborne::airActions"), mode_(core.r[5]) {}

  void run() {
    if (result(kFun80057A68, tomba_, mode_) != 0u || mode_ == 2u) {
      return;
    }
    const bool free = mode_ == 0u && u8(0x16Au) == 0u;
    if (u8(kLanded) == 0u && has(SuitAbility::Stomp) && free && result(tomba_call::kJumpPressed) != 0u) {
      setU16(kGroundSpeed, 0);
      setU8(tomba_field::kMode, kStompMode);
      if (u8(kAirState) == 2u) {
        setU8(tomba_field::kSubstate, 1);
        call(kWalkStart, tomba_, kFastFallPose, 4u);
      } else {
        setU8(tomba_field::kSubstate, 0);
      }
      return;
    }
    if (free && result(kFun80055634, tomba_, 0u) != 0u) {
      setU8(tomba_field::kSubstate, 0);
      setU8(kFloor, (u8(kFloor) & 0xEu) | u8(kSide));
      if (u8(kLanded) != 0u) {
        setU8(tomba_field::kMode, 5);
        setU8(kPhase, 0);
      } else {
        setU8(tomba_field::kMode, 6);
      }
      return;
    }
    if (u8(kLanded) == 0u || result(kFun800532A0, tomba_) != 0u) {
      return;
    }
    setU8(kPhase, 0);
    setU8(kFloor, (u8(kFloor) & 0xEu) | u8(kSide));
    if ((u8(0x149u) & 2u) != 0u) {
      setU8(tomba_field::kMode, 2);
      setU8(tomba_field::kSubstate, 2);
      call(kWalkStart, tomba_, 0x17u, 1u);
    } else {
      setU8(tomba_field::kMode, 1);
      setU8(tomba_field::kSubstate, 0);
      call(kFun80054E80, tomba_, 1u);
    }
  }

private:
  std::uint32_t mode_;
};

class Drop : public TombaStep {
public:
  explicit Drop(Core &core) : TombaStep(core, "Airborne::drop"), grounded_(core.r[5]) {}

  void run() {
    if ((u8(kLanded) | u8(kAirState)) != 0u) {
      if (s16(kDropVelocity) > kHardLanding) {
        call(kFun8005314C, tomba_, grounded_);
      }
      setU16(kDropVelocity, 0);
      setU8(0x148u, 0);
      return;
    }
    if (core_.mem_r8(kGameOverLatch) == 0u && core_.mem_r8(kRejump) != 0u) {
      call(tomba_call::kStartFall, tomba_, 0u);
      return;
    }
    setU16(kDropVelocity, u16(kDropVelocity) + kDropGravity);
    if (s16(kDropVelocity) > kTerminalVelocity) {
      setU16(kDropVelocity, kTerminalVelocity);
    }
    if (s16(kDropVelocity) >= kDropFallStart) {
      setU8(0x148u, 1);
      if (grounded_ == 0u && u8(0x146u) != 4u) {
        call(tomba_call::kStartFall, tomba_, 0u);
        const std::int16_t speed = s16(kDropVelocity);
        setU16(kVelocityY,
               has(SuitAbility::Glide) ? static_cast<std::uint16_t>(speed / 4) : static_cast<std::uint16_t>(speed));
        setU16(kDropVelocity, 0);
      }
    }
    const auto offset = static_cast<std::uint32_t>(static_cast<std::int32_t>(s16(kDropVelocity)) * 0x100);
    core_.mem_w32(tomba_ + kY, core_.mem_r32(tomba_ + kY) + offset);
  }

private:
  std::uint32_t grounded_;
};

} // namespace

void Airborne::step(Core *core) {
  psx::cpu::PreservedReturnAddress returnAddress(*core);
  psx::cpu::GuestFrameScope frame(*core, core->r[psx::cpu::kSp] - kFrameSize);
  Step(*core).run();
}

void Airborne::launch(Core *core) {
  Launch(*core).run();
}

void Airborne::airActions(Core *core) {
  psx::cpu::PreservedReturnAddress returnAddress(*core);
  psx::cpu::GuestFrameScope frame(*core, core->r[psx::cpu::kSp] - 0x20u);
  AirActions(*core).run();
}

void Airborne::drop(Core *core) {
  psx::cpu::PreservedReturnAddress returnAddress(*core);
  psx::cpu::GuestFrameScope frame(*core, core->r[psx::cpu::kSp] - kDropFrameSize);
  Drop(*core).run();
}

void Airborne::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declare(kStep, "Airborne::step", &Airborne::step);
  natives.declare(tomba_call::kDropStep, "Airborne::drop", &Airborne::drop);
  natives.declare(kLaunch, "Airborne::launch", &Airborne::launch);
  natives.declare(kAirActions, "Airborne::airActions", &Airborne::airActions);
}

} // namespace tomba
