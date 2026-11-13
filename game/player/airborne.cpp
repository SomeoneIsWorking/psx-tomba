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
constexpr std::uint32_t kFun80055F48 = 0x80055F48u;
constexpr std::uint32_t kFun800541F4 = 0x800541F4u;
constexpr std::uint32_t kFun800543C0 = 0x800543C0u;
constexpr std::uint32_t kFun80056E08 = 0x80056E08u;
constexpr std::uint32_t kFun80056F3C = 0x80056F3Cu;
constexpr std::uint32_t kFun80056EC8 = 0x80056EC8u;
constexpr std::uint32_t kFun800538E0 = 0x800538E0u;
constexpr std::uint32_t kFun8005314C = 0x8005314Cu; // the landing impact
constexpr std::uint32_t kFun80056D44 = 0x80056D44u; // (Tomba, 0): starts the fall
constexpr std::uint32_t kApproach = 0x800776F8u;    // (target, value, step) -> value moved toward target

// 0x800BF848: nonzero, a held jump restarts the take-off in the air (bits 0..1 during the rise).
constexpr std::uint32_t kRejump = 0x800BF848u;
constexpr std::uint32_t kGameOverLatch = 0x800BF80Du;
constexpr std::uint32_t kPadHeld = 0x800E7E68u;
constexpr std::uint32_t kJumpButton = 0x1F800172u;

constexpr std::uint16_t kBirdFlutter = 0x20u;
constexpr std::uint16_t kGlide = 0x40u;
constexpr std::uint16_t kWingedSuits = 0x60u;
constexpr std::uint16_t kPigSuits = 0x180u;

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
  call(kFun80055F48, tomba_, 1u);
  if (u8(0x165u) != 0u) {
    accelerate(static_cast<std::uint16_t>(-0x580));
  }
  if (u8(0x78u) == 6u) {
    accelerate(static_cast<std::uint16_t>(-0x1000));
  }
}

void Step::rise() {
  call(kFun800541F4, tomba_, 0u);
  call(kFun800543C0, tomba_);
  if (airControl() && has(kWingedSuits)) {
    riseWithWings();
  } else {
    const bool pig = (mode_ & 1u) == 0u && has(kPigSuits);
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
    call(kFun80056E08, tomba_);
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
  if (has(kGlide)) {
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
  if (has(kBirdFlutter)) {
    call(kFun80056F3C, tomba_);
  }
  if (airControl() && (has(kGlide) || u8(0x179u) == 1u)) {
    if (fallWithGlide() != 0u) {
      return;
    }
  } else {
    call(kFun800541F4, tomba_, 0u);
    if (u8(kLanded) != 0u) {
      land();
      return;
    }
    if (has(kWingedSuits)) {
      if (mode_ != kStillMode) {
        approachGroundSpeed(0u, 0x40u);
      }
      if (has(kBirdFlutter)) {
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
  call(kFun800541F4, tomba_, 0u);
  if (u8(kLanded) != 0u) {
    return 0;
  }
  std::uint32_t cap = fallCap();
  if (has(kGlide)) {
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
  if (has(kGlide)) {
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
      call(kFun80056D44, tomba_, 0u);
      return;
    }
    setU16(kDropVelocity, u16(kDropVelocity) + kDropGravity);
    if (s16(kDropVelocity) > kTerminalVelocity) {
      setU16(kDropVelocity, kTerminalVelocity);
    }
    if (s16(kDropVelocity) >= kDropFallStart) {
      setU8(0x148u, 1);
      if (grounded_ == 0u && u8(0x146u) != 4u) {
        call(kFun80056D44, tomba_, 0u);
        const std::int16_t speed = s16(kDropVelocity);
        setU16(kVelocityY, has(kGlide) ? static_cast<std::uint16_t>(speed / 4) : static_cast<std::uint16_t>(speed));
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

void Airborne::drop(Core *core) {
  psx::cpu::PreservedReturnAddress returnAddress(*core);
  psx::cpu::GuestFrameScope frame(*core, core->r[psx::cpu::kSp] - kDropFrameSize);
  Drop(*core).run();
}

void Airborne::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declare(kStep, "Airborne::step", &Airborne::step);
  natives.declare(tomba_call::kDropStep, "Airborne::drop", &Airborne::drop);
}

} // namespace tomba
