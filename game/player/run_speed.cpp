#include "player/run_speed.h"

#include "core.h"
#include "overrides/native_override_catalog.h"
#include "player/tomba_step.h"

#include <cstdint>
#include <cstdlib>

namespace tomba {
namespace {

using tomba_field::kAirState;

constexpr std::uint32_t kRunSpeedStep = 0x80055FBCu;
constexpr std::uint32_t kSpeedRows = 0x800A455Cu;   // {accel, step, cap, brake} by G+0x6D
constexpr std::uint32_t kBoostedRow = 0x800A4574u;  // with G+0x174 & 0x30
constexpr std::uint32_t kForcedSlowA = 0x800BF816u; // both set: the cap is 0xC0F
constexpr std::uint32_t kForcedSlowB = 0x800BF84Du;

constexpr std::uint32_t kSpeedX = 0x44u;
constexpr std::uint32_t kWall = 0x5Fu; // bit 0 the wall side, bit 3 a wall to push off
constexpr std::uint32_t kTurnFrames = 0x96u;
constexpr std::uint32_t kDirection = 0x149u; // last direction; 4/5 the turn skid
constexpr std::uint32_t kSlowWalk = 0x14Bu;
constexpr std::uint32_t kCap = 0x15Eu;
constexpr std::uint32_t kCarrying = 0x165u;

constexpr std::uint16_t kSnowFloor = 0x500u;
constexpr std::int32_t kWallPush = 0xD00;
constexpr std::int16_t kSlowCap = 0xC0F;
constexpr std::int16_t kSlowWalkCap = 0x110F;
constexpr std::int32_t kBrakeMargin = 0x240;
constexpr std::int32_t kAirBrakeExtra = 0x80;
constexpr std::int32_t kTurnFrameBias = 7;

std::int16_t trunc16(std::int32_t value) {
  return static_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

// The guest's signed division by a power of two, rounding toward zero.
std::int32_t divToZero(std::int32_t value, std::int32_t shift) {
  return (value < 0 ? value + (1 << shift) - 1 : value) >> shift;
}

class Step : public TombaStep {
public:
  explicit Step(Core &core) : TombaStep(core, "RunSpeed::step"), direction_(core.r[5] & 0xFFu) {}

  void run() {
    const std::uint32_t row =
        (u8(0x174u) & 0x30u) != 0u ? kBoostedRow : kSpeedRows + static_cast<std::uint32_t>(u8(0x6Du)) * 8u;
    accel_ = row16(row, 0);
    step_ = row16(row, 1);
    cap_ = row16(row, 2);
    brake_ = row16(row, 3);
    brakeDivisor_ = brake_;
    hardBrake_ = trunc16(brake_ - kBrakeMargin);
    slip_ = slipsOnSnow() && (u16(tomba_field::kFloorAttribute) & 0xF00u) == kSnowFloor;
    applyCap(row);
    if (slip_) {
      accel_ = trunc16(divToZero(accel_, 3));
      brake_ = trunc16(divToZero(brake_, 1));
      hardBrake_ = trunc16(divToZero(hardBrake_, 1));
    }
    airborne_ = u8(kAirState) != 0u;
    if (airborne_) {
      step_ = trunc16(static_cast<std::uint16_t>(step_) << (has(SuitAbility::AirControl) ? 4 : 1));
    }
    switch (direction_ & 3u) {
    case 0:
      forward(1);
      return;
    case 1:
      forward(-1);
      return;
    default:
      coast(row);
      return;
    }
  }

private:
  std::int16_t row16(std::uint32_t row, std::uint32_t index) const {
    return static_cast<std::int16_t>(core_.mem_r16(row + index * 2u));
  }

  void applyCap(std::uint32_t row) {
    if (core_.mem_r8(kForcedSlowA) != 0u && core_.mem_r8(kForcedSlowB) != 0u) {
      setU16(kCap, static_cast<std::uint16_t>(kSlowCap));
      cap_ = kSlowCap;
      return;
    }
    if (u8(kCarrying) != 0u) {
      accel_ = trunc16((accel_ * 19) >> 4);
      step_ = trunc16(step_ * 3);
      cap_ = trunc16((cap_ * 19) >> 4);
      setU16(kCap, static_cast<std::uint16_t>((row16(row, 2) * 19) >> 4));
      return;
    }
    const auto slowWalk = static_cast<std::int8_t>(u8(kSlowWalk));
    std::int16_t cap = cap_;
    if (slowWalk == 1) {
      cap = kSlowCap;
    } else if (slowWalk == 2) {
      cap = kSlowWalkCap;
    }
    setU16(kCap, static_cast<std::uint16_t>(cap));
  }

  void store(std::int32_t value) {
    setU16(kSpeedX, static_cast<std::uint16_t>(value));
  }
  std::int16_t speed() const {
    return s16(kSpeedX);
  }
  // Adds `delta` to the speed and returns the stored value.
  std::int16_t add(std::int32_t delta) {
    const std::int16_t value = trunc16(speed() + delta);
    store(value);
    return value;
  }

  // Direction 0 (sign 1) or 1 (sign -1): run that way. The guest's two halves are mirror images.
  void forward(std::int32_t sign) {
    sign_ = sign;
    self_ = sign > 0 ? 0u : 1u;
    const std::int32_t self = self_;
    const std::uint8_t wall = u8(kWall);
    if (wall != 0u && (wall & 1u) == self) {
      wallPush(wall);
      return;
    }
    const std::uint8_t last = u8(kDirection);
    if ((last & 2u) != 0u) {
      if ((last & 1u) == self) {
        add(sign * accel_);
      }
      setU8(kDirection, self);
      return;
    }
    if (last == static_cast<std::uint8_t>((self ^ 1) | 4)) {
      skid();
      return;
    }
    const std::int32_t along = sign * speed();
    if (along > 0) {
      runOn(sign);
    } else if (turned(last, along)) {
      return;
    } else {
      start(sign);
    }
    setU8(kDirection, self);
  }

  // A wall on this side (G+0x5F): bit 3 pushes off to the fixed wall speed, else the speed is cut to zero.
  void wallPush(std::uint8_t wall) {
    const std::int32_t sign = sign_;
    setU8(kDirection, self_);
    if ((wall & 8u) == 0u) {
      if (sign * add(-sign * brake_) < 0) {
        store(0);
      }
      return;
    }
    if (sign * speed() > kWallPush) {
      if (sign * add(-sign * hardBrake_) < kWallPush) {
        store(sign * kWallPush);
      }
    } else if (sign * add(sign * step_) > kWallPush) {
      store(sign * kWallPush);
    }
  }

  // The turn skid (G+0x149 = 4 | direction) running out against the new direction.
  void skid() {
    const std::int32_t sign = sign_;
    const std::int16_t value = add(sign * brake_);
    const std::int32_t limit = slip_ ? -divToZero(cap_, 1) : 0;
    if (sign * value < limit) {
      return;
    }
    if (!airborne_) {
      store(slip_ ? sign * divToZero(cap_, 2) : sign * cap_);
    }
    setU8(kDirection, self_);
  }

  // Already moving this way: at least the start speed, then approach the cap.
  void runOn(std::int32_t sign) {
    const std::int32_t along = sign * speed();
    const std::int16_t cap = s16(kCap);
    if (along < accel_) {
      store(sign * accel_);
    } else if (cap < along) {
      if (sign * add(-sign * hardBrake_) < cap) {
        store(sign * cap);
      }
    } else if (cap < sign * add(sign * step_)) {
      store(sign * cap);
    }
  }

  // A fast reverse on the ground starts the skid (G+0x149 = 4 | the old direction) for |speed| / brake - 7 frames.
  bool turned(std::uint8_t last, std::int32_t along) {
    const std::uint8_t opposite = self_ ^ 1u;
    if (airborne_ || last != opposite || along > -divToZero(cap_ * 3, 2)) {
      return false;
    }
    const std::int32_t frames = std::abs(static_cast<std::int32_t>(speed())) / brakeDivisor_ - kTurnFrameBias;
    setU8(kDirection, opposite | 4u);
    setU16(kTurnFrames, static_cast<std::uint16_t>(frames));
    if (trunc16(frames) <= 0) {
      setU16(kTurnFrames, 1u);
    }
    add(sign_ * brake_);
    return true;
  }

  // Standing or moving the other way: accelerate by the brake step, or in the air by a winged suit's steer.
  void start(std::int32_t sign) {
    add(sign * (airborne_ && has(SuitAbility::AirControl) ? step_ : brake_));
  }

  // No direction: brake toward zero; 8 with a winged suit in the air brakes harder.
  void coast(std::uint32_t row) {
    std::int32_t brake = brake_;
    if ((direction_ & 8u) != 0u && has(SuitAbility::AirControl) && airborne_) {
      brake = static_cast<std::int32_t>(core_.mem_r16(row + 6u)) + kAirBrakeExtra;
    } else if ((u8(kWall) & 3u) != (direction_ & 3u) && airborne_) {
      brake = hardBrake_;
      setU8(kDirection, direction_ & 1u);
    }
    if (speed() > 0) {
      if (add(-brake) > 0) {
        return;
      }
    } else if (add(brake) < 0) {
      return;
    }
    store(0);
    setU8(kDirection, direction_ & 3u);
  }

  std::uint32_t direction_;
  std::int32_t sign_ = 1;
  std::uint8_t self_ = 0;
  std::int16_t accel_ = 0;
  std::int16_t step_ = 0;
  std::int16_t cap_ = 0;
  std::int16_t brake_ = 0;
  std::int16_t brakeDivisor_ = 0;
  std::int16_t hardBrake_ = 0;
  bool airborne_ = false;
  bool slip_ = false;
};

} // namespace

void RunSpeed::step(Core *core) {
  Step(*core).run();
}

void RunSpeed::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declare(kRunSpeedStep, "RunSpeed::step", &RunSpeed::step);
}

} // namespace tomba
