#include "player/updraft_contact.h"

#include "guest_abi.h"
#include "overrides/native_override_catalog.h"
#include "player/tomba_step.h"

#include <cstdint>

namespace tomba {
namespace {

using namespace tomba_field;

constexpr std::uint32_t kSteamVent = 0x8010D90Cu;
constexpr std::uint32_t kArea11Updraft = 0x8010A93Cu;
constexpr std::uint32_t kContactFrameSize = 0x28u;

constexpr std::uint32_t kGameOverLatch = 0x800BF80Du;
constexpr std::uint32_t kPauseLatch = 0x800BF809u;
constexpr std::uint32_t kEventLatch = 0x800BF80Au;
constexpr std::uint32_t kWaterSound = 0x800BF81Eu;
constexpr std::uint32_t kLiftKind = 0x800BF848u;
constexpr std::uint32_t kColdPowder = 0x800BFAD5u;
constexpr std::uint32_t kAirLock = 0x1F800137u;
constexpr std::uint32_t kCarryLock = 0x1F800259u;
constexpr std::uint32_t kBoxFloor = 0x1F800090u; // set by the box touch: the floor kind under Tomba
constexpr std::uint32_t kBoxSide = 0x1F800094u;  // set by the box touch: a side hit, not a top one

constexpr std::uint32_t kBoxTouch = 0x80022F04u;  // (Tomba, actor): nonzero on contact
constexpr std::uint32_t kKnockBack = 0x80022D08u; // (Tomba, actor, a2, a3): nonzero when it took

constexpr std::uint32_t kSpeedX = 0x44u;
constexpr std::uint32_t kTopExtent = 0x84u;
constexpr std::uint32_t kBottomExtent = 0x86u;
constexpr std::uint32_t kTouchFrames = 0x64u;

constexpr std::uint8_t kUnderShape = 8u;
constexpr std::uint8_t kLiftSound = 0x25u;
constexpr std::uint8_t kLiftAnimation = 0x12u;
constexpr std::uint8_t kFreezeSubstate = 6u;
constexpr std::int32_t kFreezeFrames = 0x3D;

class Contact : public TombaStep {
public:
  Contact(Core &core, const char *owner) : TombaStep(core, owner), top_(core.r[psx::cpu::kA1]) {}

protected:
  std::uint16_t topU16(std::uint32_t field) const {
    return core_.mem_r16(top_ + field);
  }
  void setTopU16(std::uint32_t field, std::uint32_t value) {
    core_.mem_w16(top_ + field, static_cast<std::uint16_t>(value));
  }

  void damp() {
    const std::uint16_t speed = u16(kSpeedX);
    setU16(kSpeedX, speed - (static_cast<std::int16_t>(speed) >> 2));
  }

  // The extents grow while Tomba is tested against them; restore() puts them back.
  void hold() {
    top_extent_ = topU16(kTopExtent);
    bottom_extent_ = topU16(kBottomExtent);
  }
  void grow() {
    hold();
    const std::uint32_t reach = has(SuitAbility::Updraft) ? 400u : 200u;
    setTopU16(kTopExtent, top_extent_ + reach);
    setTopU16(kBottomExtent, topU16(kBottomExtent) + reach);
  }
  void restore() {
    setTopU16(kTopExtent, top_extent_);
    setTopU16(kBottomExtent, bottom_extent_);
  }

  bool touches() {
    return result(kBoxTouch, tomba_, top_) != 0u;
  }

  // The overlap's depth kicks Tomba up; a side hit only pushes him back out.
  void lift() {
    core_.mem_w8(kWaterSound, 0);
    const std::uint32_t depth = (u16(kTopExtent) + topU16(kTopExtent) + 300u) - (topU16(kY + 2u) - u16(kY + 2u));
    const std::uint32_t floor = core_.mem_r32(kBoxFloor);
    if (floor != 0u) {
      core_.mem_w8(kLiftKind, static_cast<std::uint8_t>(floor));
    } else {
      core_.mem_w8(kLiftKind, 1);
      if (u8(kHold) == 0u) {
        call(tomba_call::kSfxTrigger, kLiftSound, 0u, 0u);
      }
    }
    damp();
    if (core_.mem_r32(kBoxSide) != 0u) {
      setU16(kDropVelocity, 0);
      setU16(kVelocityY, 0);
      core_.mem_w32(tomba_ + kY, core_.mem_r32(tomba_ + kY) - (static_cast<std::int32_t>(depth << 16u) >> 4));
      return;
    }
    const auto velocity = static_cast<std::uint16_t>(u16(kVelocityY) - (depth << 1u));
    setU16(kVelocityY, velocity);
    const std::int32_t rise = static_cast<std::int16_t>(velocity);
    if (rise >= 0x281) {
      setU16(kVelocityY, 0x280u);
    } else if (static_cast<std::int16_t>(depth) < 500) {
      if (rise < -0x3E00) {
        setU16(kVelocityY, static_cast<std::uint16_t>(-0x3E00));
      }
    } else if (rise < -0x2800) {
      setU16(kVelocityY, static_cast<std::uint16_t>(-0x2800));
    }
    const std::uint8_t animation = u8(kAnimation);
    if (static_cast<std::uint32_t>(animation - 0x13u) < 3u || animation == 0xCAu || animation == 0xE5u ||
        animation == 0xE6u) {
      if (core_.mem_r32(kBoxFloor) == 0u || u8(kAirState) != 1u) {
        call(tomba_call::kSetAnimation, tomba_, kLiftAnimation, 4u);
      } else {
        core_.mem_w8(kLiftKind, 4);
      }
    }
  }

  std::uint32_t top_;

private:
  std::uint16_t top_extent_ = 0;
  std::uint16_t bottom_extent_ = 0;
};

// A01 FUN_8010D90C.
class SteamVent : public Contact {
public:
  explicit SteamVent(Core &core) : Contact(core, "UpdraftContact::steamVent") {}

  void run() {
    if (core_.mem_r8(kGameOverLatch) != 0u || core_.mem_r8(kEventLatch) != 0u) {
      return;
    }
    if (core_.mem_r8(top_ + 3u) == kUnderShape) {
      pushDown();
      return;
    }
    if (u8(kHold) == 0u) {
      grow();
    } else {
      hold();
    }
    if (touches()) {
      lift();
    }
    restore();
  }

private:
  void pushDown() {
    if (!touches() || core_.mem_r32(kBoxSide) != 0u) {
      return;
    }
    const std::uint32_t depth = (u16(kBottomExtent) - u16(kTopExtent)) + (topU16(kBottomExtent) - topU16(kTopExtent)) +
                                (topU16(kY + 2u) - u16(kY + 2u));
    damp();
    core_.mem_w32(tomba_ + kY, core_.mem_r32(tomba_ + kY) + (static_cast<std::int32_t>(depth << 16u) >> 4));
    core_.mem_w8(kWaterSound, 0);
  }
};

// A0B FUN_8010A93C.
class Area11Updraft : public Contact {
public:
  explicit Area11Updraft(Core &core) : Contact(core, "UpdraftContact::area11Updraft") {}

  void run() {
    if (core_.mem_r8(kGameOverLatch) != 0u || core_.mem_r8(kCarryLock) != 0u || core_.mem_r8(kPauseLatch) != 0u ||
        core_.mem_r8(kEventLatch) != 0u || (u8(0) & 0xCu) != 0u) {
      return;
    }
    grow();
    if (!touches()) {
      // The grown extents stay, as in retail.
      setTopU16(kTouchFrames, 0);
      return;
    }
    lift();
    const auto frames = static_cast<std::uint16_t>(topU16(kTouchFrames) + 1u);
    setTopU16(kTouchFrames, frames);
    if (static_cast<std::int16_t>(frames) >= kFreezeFrames && s16(0x16Eu) != 0 && core_.mem_r8(kAirLock) == 0u &&
        (u8(0x177u) & 2u) == 0u && u8(kHold) == 0u && !has(SuitAbility::FreezeImmunity)) {
      freeze();
    }
    core_.mem_w8(top_ + 0x5Eu, 1);
    restore();
  }

private:
  void freeze() {
    const bool held = core_.mem_r8(kColdPowder) == 0u && result(kKnockBack, tomba_, 0u, 1u, 1u) != 0u;
    setU8(kPhase, held ? 0u : 1u);
    setU8(kGroup, 2);
    setU8(kMode, kFreezeSubstate);
    setU8(kSubstate, 0);
    setU8(0, u8(0) | 0xCu);
    core_.mem_w8(top_, 2);
    setTopU16(0x40u, 0);
  }
};

} // namespace

void UpdraftContact::steamVent(Core *core) {
  psx::cpu::PreservedReturnAddress returnAddress(*core);
  psx::cpu::GuestFrameScope frame(*core, core->r[psx::cpu::kSp] - kContactFrameSize);
  SteamVent(*core).run();
}

void UpdraftContact::area11Updraft(Core *core) {
  psx::cpu::PreservedReturnAddress returnAddress(*core);
  psx::cpu::GuestFrameScope frame(*core, core->r[psx::cpu::kSp] - kContactFrameSize);
  Area11Updraft(*core).run();
}

void UpdraftContact::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declareOverlay(
      "A01", kSteamVent, "UpdraftContact::steamVent", &UpdraftContact::steamVent, psx::cpu::NativeReturn::Void);
  natives.declareOverlay("A0B",
                         kArea11Updraft,
                         "UpdraftContact::area11Updraft",
                         &UpdraftContact::area11Updraft,
                         psx::cpu::NativeReturn::Void);
}

} // namespace tomba
