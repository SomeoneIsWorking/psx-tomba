// game/player/tomba_step.h — the shared frame of a native Tomba step: his fields, guest calls and suit gates.
#pragma once

#include "core.h"
#include "guest_call.h"
#include "player/suit_abilities.h"

#include <cstdint>
#include <lucent/log.h>

namespace tomba {

// Fields of Tomba's block G that the native steps share.
namespace tomba_field {
inline constexpr std::uint32_t kGroup = 4u;    // G+4: 1 the action modes, 2 the hazard substates
inline constexpr std::uint32_t kMode = 5u;     // G+5: the action mode FUN_80058918 dispatches
inline constexpr std::uint32_t kSubstate = 6u; // G+6: the mode's own state
inline constexpr std::uint32_t kPhase = 7u;    // G+7: the airborne phase
inline constexpr std::uint32_t kTarget = 0x10u;
inline constexpr std::uint32_t kLanded = 0x29u;
inline constexpr std::uint32_t kX = 0x2Cu;
inline constexpr std::uint32_t kY = 0x30u;
inline constexpr std::uint32_t kZ = 0x34u;
inline constexpr std::uint32_t kAnimationData = 0x38u;
inline constexpr std::uint32_t kAnimation = 0x46u;
inline constexpr std::uint32_t kVelocityY = 0x4Au;
inline constexpr std::uint32_t kDropVelocity = 0x50u;
inline constexpr std::uint32_t kGroundSpeed = 0x58u;
inline constexpr std::uint32_t kLean = 0x62u;
inline constexpr std::uint32_t kFloorAttribute = 0x6Au;
inline constexpr std::uint32_t kHold = 0x78u;
inline constexpr std::uint32_t kApexLatch = 0x144u;
inline constexpr std::uint32_t kAirState = 0x145u;
inline constexpr std::uint32_t kSide = 0x147u;
inline constexpr std::uint32_t kFloor = 0x14Au;
inline constexpr std::uint32_t kAirFrames = 0x178u;
} // namespace tomba_field

// Guest functions the native steps call.
namespace tomba_call {
inline constexpr std::uint32_t kAirborneStep = 0x800574E0u; // Tomba_airborneStep (Tomba, mode)
inline constexpr std::uint32_t kDropStep = 0x80056C00u;     // Tomba_dropStep (Tomba, stay grounded)
inline constexpr std::uint32_t kGroundCheck = 0x8005444Cu;  // Tomba_groundCheck
inline constexpr std::uint32_t kSetAnimation = 0x80054D14u; // Engine_walkStart (actor, animation, blend)
inline constexpr std::uint32_t kAnimationStep = 0x80076D68u; // Animation_stepFramed: 1 on the last frame
inline constexpr std::uint32_t kVelocityIntegrate = 0x80056B48u;
inline constexpr std::uint32_t kSfxTrigger = 0x80074590u;
inline constexpr std::uint32_t kJumpPressed = 0x80055824u; // the jump button's press edge
inline constexpr std::uint32_t kFloorProbe = 0x80049280u;  // (actor, 0, lean): nonzero on a floor
inline constexpr std::uint32_t kDamage = 0x80022C0Cu;      // Tomba_damage (Tomba, points)
} // namespace tomba_call

// One native call on Tomba: a0 is G. The guest stack frame of the replaced function is entered so its locals sit
// where the original kept them.
class TombaStep {
public:
  TombaStep(Core &core, const char *owner) : core_(core), owner_(owner), tomba_(core.r[4]) {}

protected:
  std::uint8_t u8(std::uint32_t field) const {
    return core_.mem_r8(tomba_ + field);
  }
  void setU8(std::uint32_t field, std::uint32_t value) {
    core_.mem_w8(tomba_ + field, static_cast<std::uint8_t>(value));
  }
  std::uint16_t u16(std::uint32_t field) const {
    return core_.mem_r16(tomba_ + field);
  }
  std::int16_t s16(std::uint32_t field) const {
    return static_cast<std::int16_t>(core_.mem_r16(tomba_ + field));
  }
  void setU16(std::uint32_t field, std::uint32_t value) {
    core_.mem_w16(tomba_ + field, static_cast<std::uint16_t>(value));
  }
  void stepSubstate() {
    setU8(tomba_field::kSubstate, u8(tomba_field::kSubstate) + 1u);
  }
  bool has(std::uint16_t bits) const {
    const bool granted = SuitAbilities::has(core_, bits);
    lucent::debug("suits",
                  "{} asks {:04X}: {} (worn {:04X}, mode {:02X}/{:02X})",
                  owner_,
                  bits,
                  granted,
                  u16(0x17Eu),
                  u8(tomba_field::kMode),
                  u8(tomba_field::kSubstate));
    return granted;
  }
  template <typename... Args> void call(std::uint32_t address, Args... args) {
    psx::cpu::callGuestNow(core_, owner_, address, args...);
  }
  template <typename... Args> std::uint32_t result(std::uint32_t address, Args... args) {
    return psx::cpu::callGuestNowResult(core_, owner_, address, args...);
  }
  std::int32_t signedResult(std::uint32_t address, std::uint32_t a0) {
    return static_cast<std::int32_t>(result(address, a0));
  }

  Core &core_;
  const char *owner_;
  std::uint32_t tomba_;
};

} // namespace tomba
