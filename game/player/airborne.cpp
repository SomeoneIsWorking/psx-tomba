#include "player/airborne.h"

#include "core.h"
#include "guest_abi.h"
#include "guest_call.h"
#include "overrides/native_override_catalog.h"
#include "player/suit_abilities.h"

namespace tomba {
namespace {

constexpr std::uint32_t kStep = 0x800574E0u;
constexpr std::uint32_t kFrameSize = 0x30u;
constexpr std::uint32_t kLandingTarget = 0x10u; // sp offset of the landing position vector

constexpr std::uint32_t kFun8005749C = 0x8005749Cu;
constexpr std::uint32_t kFun80055F48 = 0x80055F48u;
constexpr std::uint32_t kFun800541F4 = 0x800541F4u;
constexpr std::uint32_t kFun800543C0 = 0x800543C0u;
constexpr std::uint32_t kFun80056E08 = 0x80056E08u;
constexpr std::uint32_t kFun80056F3C = 0x80056F3Cu;
constexpr std::uint32_t kFun80056EC8 = 0x80056EC8u;
constexpr std::uint32_t kFun80049280 = 0x80049280u;
constexpr std::uint32_t kGroundCheck = 0x8005444Cu; // Tomba_groundCheck
constexpr std::uint32_t kFun800538E0 = 0x800538E0u;
constexpr std::uint32_t kFun8005314C = 0x8005314Cu;
constexpr std::uint32_t kWalkStart = 0x80054D14u; // Engine_walkStart (actor, animation, blend): sets the animation
constexpr std::uint32_t kSfxTrigger = 0x80074590u; // Sfx_trigger
constexpr std::uint32_t kApproach = 0x800776F8u; // (target, value, step) -> value moved toward target

// 0x800BF848: nonzero, a held jump restarts the take-off in the air (bits 0..1 during the rise).
constexpr std::uint32_t kRejump = 0x800BF848u;
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

// Tomba's fields (G block).
constexpr std::uint32_t kPhase = 7u;
constexpr std::uint32_t kLanded = 0x29u;
constexpr std::uint32_t kX = 0x2Cu;
constexpr std::uint32_t kY = 0x30u;
constexpr std::uint32_t kZ = 0x34u;
constexpr std::uint32_t kAnimationData = 0x38u;
constexpr std::uint32_t kAnimation = 0x46u;
constexpr std::uint32_t kFacingX = 0x48u;
constexpr std::uint32_t kVelocityY = 0x4Au;
constexpr std::uint32_t kFacingZ = 0x4Cu;
constexpr std::uint32_t kGroundSpeed = 0x58u;
constexpr std::uint32_t kLean = 0x62u;
constexpr std::uint32_t kApexLatch = 0x144u;
constexpr std::uint32_t kAirState = 0x145u;
constexpr std::uint32_t kAirFrames = 0x178u;
constexpr std::uint32_t kFloor = 0x14Au;

class Step {
public:
  // The guest call's arguments: a0 Tomba, a1 the mode.
  explicit Step(Core &core) : core_(core), tomba_(core.r[4]), mode_(core.r[5]) {}

  void run();

private:
  std::uint8_t u8(std::uint32_t field) const {
    return core_.mem_r8(tomba_ + field);
  }
  void setU8(std::uint32_t field, std::uint32_t value) {
    core_.mem_w8(tomba_ + field, static_cast<std::uint8_t>(value));
  }
  std::int16_t s16(std::uint32_t field) const {
    return static_cast<std::int16_t>(core_.mem_r16(tomba_ + field));
  }
  bool has(std::uint16_t bits) const {
    return SuitAbilities::has(core_, bits);
  }
  bool jumpHeld() const {
    return (core_.mem_r16(kPadHeld) & core_.mem_r16(kJumpButton)) != 0u;
  }
  bool airControl() const {
    return (mode_ & 0x10u) == 0u && core_.mem_r8(kRejump) == 0u && u8(0x61u) == 0u;
  }
  template <typename... Args> void call(std::uint32_t address, Args... args) {
    psx::cpu::callGuestNow(core_, "Airborne::step", address, args...);
  }
  template <typename... Args> std::uint32_t result(std::uint32_t address, Args... args) {
    return psx::cpu::callGuestNowResult(core_, "Airborne::step", address, args...);
  }
  // Velocity += gravity as the u16 the guest keeps; returns the new value.
  std::int16_t accelerate(std::uint16_t gravity) {
    const auto velocity = static_cast<std::uint16_t>(core_.mem_r16(tomba_ + kVelocityY) + gravity);
    core_.mem_w16(tomba_ + kVelocityY, velocity);
    return static_cast<std::int16_t>(velocity);
  }
  void moveByVelocity() {
    const auto offset = static_cast<std::uint32_t>(static_cast<std::int32_t>(s16(kVelocityY)) * 0x100);
    core_.mem_w32(tomba_ + kY, core_.mem_r32(tomba_ + kY) + offset);
  }
  void approachGroundSpeed(std::uint32_t target, std::uint32_t rate) {
    const std::uint32_t speed =
        result(kApproach, target, static_cast<std::uint32_t>(static_cast<std::int32_t>(s16(kGroundSpeed))), rate);
    core_.mem_w16(tomba_ + kGroundSpeed, static_cast<std::uint16_t>(speed));
  }
  std::uint32_t landingProbe() {
    const auto lean = static_cast<std::uint32_t>(static_cast<std::int32_t>(s16(kLean)));
    const std::int32_t slope = static_cast<std::int32_t>(lean << 17) >> 16;
    const std::uint32_t landed = result(kFun80049280, tomba_, 0u, slope);
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

  void takeOff();
  void rise();
  void riseWithWings();
  void fall();
  std::uint32_t fallWithGlide();
  std::uint32_t fallCap();
  void restartTakeOff();
  void land();

  Core &core_;
  std::uint32_t tomba_;
  std::uint32_t mode_;
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
      core_.mem_w16(tomba_ + kVelocityY, static_cast<std::uint16_t>(kTerminalVelocity));
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
    core_.mem_w16(tomba_ + kVelocityY, static_cast<std::uint16_t>(cap));
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

} // namespace

void Airborne::step(Core *core) {
  psx::cpu::PreservedReturnAddress returnAddress(*core);
  psx::cpu::GuestFrameScope frame(*core, core->r[psx::cpu::kSp] - kFrameSize);
  Step(*core).run();
}

void Airborne::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declare(kStep, "Airborne::step", &Airborne::step);
}

} // namespace tomba
