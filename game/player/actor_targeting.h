// game/player/actor_targeting.h — can the actor ACQUIRE this target: near enough, and in the right
// direction? See actor_targeting.cpp for the evidence. Lenses only; setters are ONE-LINE single
// statements so tools/dynamic differential evidence can count their stores (it harvests them by regex from
// game/**/*.h and stops counting a setter that grows a second statement).
#pragma once
#include "core.h"
#include <cstdint>

class Game;

// Both sides of the test share this field block — the actor and the target are read through the SAME
// offsets, which is itself evidence they are the same record shape. +0x2E/+0x36 are the integer world
// X/Z, +0x80 each body's half of the reach, +0x84/+0x86 the vertical offset and band, and +0x145 the
// walk/jump state byte CollisionResolve::classifyBodyContact also gates on.
namespace targeting {
constexpr uint32_t kTypeByte = 2;    // u8  — on the TARGET: selects which value gets published
constexpr uint32_t kContact = 43;    // 0x2B — u8, the shared contact byte
constexpr uint32_t kPosX = 46;       // 0x2E — u16
constexpr uint32_t kPosY = 50;       // 0x32 — u16
constexpr uint32_t kPosZ = 54;       // 0x36 — u16
constexpr uint32_t kGuardByte = 97;  // 0x61 — u8, 128 suppresses the global write
constexpr uint32_t kReach = 128;     // 0x80 — s16
constexpr uint32_t kHeightOff = 132; // 0x84 — u16
constexpr uint32_t kBandY = 134;     // 0x86 — s16
constexpr uint32_t kHeading = 320;   // 0x140 — u16, the direction the arc test is measured against
constexpr uint32_t kWalkState = 325; // 0x145 — u8, must be 0 to acquire anything
} // namespace targeting

struct TargetingBody {
  Core *mCore;
  uint32_t mAt;

  uint32_t typeByte() const {
    return mCore->mem_r8(mAt + targeting::kTypeByte);
  }
  uint32_t guardByte() const {
    return mCore->mem_r8(mAt + targeting::kGuardByte);
  }
  uint32_t walkState() const {
    return mCore->mem_r8(mAt + targeting::kWalkState);
  }
  uint32_t posX() const {
    return mCore->mem_r16(mAt + targeting::kPosX);
  }
  uint32_t posY() const {
    return mCore->mem_r16(mAt + targeting::kPosY);
  }
  uint32_t posZ() const {
    return mCore->mem_r16(mAt + targeting::kPosZ);
  }
  uint32_t heading() const {
    return mCore->mem_r16(mAt + targeting::kHeading);
  }
  uint32_t heightOff() const {
    return mCore->mem_r16(mAt + targeting::kHeightOff);
  }
  int32_t reach() const {
    return mCore->mem_r16s(mAt + targeting::kReach);
  }
  int32_t bandY() const {
    return mCore->mem_r16s(mAt + targeting::kBandY);
  }

  void setContact(uint8_t v) const {
    mCore->mem_w8(mAt + targeting::kContact, v);
  }
};

class ActorTargeting {
public:
  // 0x8001FAE0 — a0 = the actor doing the acquiring, a1 = the candidate target.
  static void tryAcquireTarget(Core *c);

  static void registerOverrides();
};
