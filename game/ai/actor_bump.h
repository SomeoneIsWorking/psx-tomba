// game/ai/actor_bump.h — the actor's BUMP RESPONSE: what happens when he runs into another object.
//
// See actor_bump.cpp for the identification evidence. Lenses only; every setter is a ONE-LINE single
// statement because tools/dynamic differential evidence harvests their mem_wN widths by regex from game/**/*.h and
// stops counting a setter that grows a second statement.
#pragma once
#include "core.h"
#include <cstdint>

class Game;

// The ACTOR side. These offsets are already named by other subsystems: +0x17E is the state-flags word
// whose bit 9 game/player/actor_tomba.h documents as "paused", +0x2B is the shared object CONTACT
// byte, and +0x2E/+0x36 are the integer world X/Z. +0x144 is the walk/jump state byte
// game/world/collision_resolve.cpp's classifyBodyContact also gates on.
namespace actorbump {
constexpr uint32_t kState = 0;          // u8  — becomes 3 on a recoil
constexpr uint32_t kPhase = 4;          // u8  — becomes 2
constexpr uint32_t kSubPhase = 5;       // u8  — becomes 2
constexpr uint32_t kTimer = 6;          // u8  — cleared
constexpr uint32_t kContact = 43;       // 0x2B — u8, stamped with the contact direction
constexpr uint32_t kPosX = 46;          // 0x2E — u16
constexpr uint32_t kPosZ = 54;          // 0x36 — u16
constexpr uint32_t kFacing = 86;        // 0x56 — s16
constexpr uint32_t kReach = 128;        // 0x80 — s16, this body's half of the push-apart distance
constexpr uint32_t kStateFlags = 382;   // 0x17E — u16
constexpr uint32_t kPausedBit = 512;    // 0x200
constexpr uint32_t kSpecialBit = 32768; // 0x8000 — selects the special interaction over the generic
constexpr uint32_t kWalkState = 324;    // 0x144 — u8
constexpr uint32_t kRecoilTimer = 370;  // 0x172 — u16, set to 120 on a recoil
constexpr uint32_t kStateBusyMask = 6;  // state bits that block a recoil entirely
} // namespace actorbump

struct BumpActor {
  Core *mCore;
  uint32_t mAt;

  bool paused() const {
    return (mCore->mem_r16(mAt + actorbump::kStateFlags) & actorbump::kPausedBit) != 0;
  }
  int32_t stateFlags() const {
    return mCore->mem_r16s(mAt + actorbump::kStateFlags);
  }
  uint32_t state() const {
    return mCore->mem_r8(mAt + actorbump::kState);
  }
  uint32_t walkState() const {
    return mCore->mem_r8(mAt + actorbump::kWalkState);
  }
  int32_t facing() const {
    return mCore->mem_r16s(mAt + actorbump::kFacing);
  }
  int32_t reach() const {
    return mCore->mem_r16s(mAt + actorbump::kReach);
  }
  uint32_t posX() const {
    return mCore->mem_r16(mAt + actorbump::kPosX);
  }
  uint32_t posZ() const {
    return mCore->mem_r16(mAt + actorbump::kPosZ);
  }

  void setPosX(uint16_t v) const {
    mCore->mem_w16(mAt + actorbump::kPosX, v);
  }
  void setPosZ(uint16_t v) const {
    mCore->mem_w16(mAt + actorbump::kPosZ, v);
  }
  void setContact(uint8_t v) const {
    mCore->mem_w8(mAt + actorbump::kContact, v);
  }
  void setPhase(uint8_t v) const {
    mCore->mem_w8(mAt + actorbump::kPhase, v);
  }
  void setSubPhase(uint8_t v) const {
    mCore->mem_w8(mAt + actorbump::kSubPhase, v);
  }
  void setState(uint8_t v) const {
    mCore->mem_w8(mAt + actorbump::kState, v);
  }
  void setTimer(uint8_t v) const {
    mCore->mem_w8(mAt + actorbump::kTimer, v);
  }
  void setRecoilTimer(uint16_t v) const {
    mCore->mem_w16(mAt + actorbump::kRecoilTimer, v);
  }
};

// The OTHER object. Only three fields are read: its state byte, its own half of the push-apart
// distance, and the position the actor is pushed out around.
struct BumpOther {
  Core *mCore;
  uint32_t mAt;

  uint32_t state() const {
    return mCore->mem_r8(mAt + actorbump::kState);
  }
  int32_t reach() const {
    return mCore->mem_r16s(mAt + actorbump::kReach);
  }
  uint32_t posX() const {
    return mCore->mem_r16(mAt + actorbump::kPosX);
  }
  uint32_t posZ() const {
    return mCore->mem_r16(mAt + actorbump::kPosZ);
  }
};

class ActorBump {
public:
  // 0x8010EA80 — a0 = the actor, a1 = the object he ran into.
  static void respondToContact(Core *c);

  static void registerOverrides();
};
