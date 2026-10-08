// game/ai/actor_object_contact.h — ACTOR-vs-OBJECT contact: did the actor hit this object, and if
// not, is he standing close enough to it to count as touching it?
//
// See actor_object_contact.cpp for the identification evidence. This header holds the two typed
// lenses the leaf needs — the actor's G-block and the object — plus a named constant per field.
//
// Write accessors are deliberately ONE-LINERS: tools/dynamic differential evidence harvests a lens setter's mem_wN
// width by regex and counts `obj.setPhase(v)` as exactly the store it performs, but ONLY for lenses
// defined in a game/**/*.h header whose setters are single statements.
#pragma once
#include "core.h"
#include <cstdint>

class Game;

// ------------------------------------------------------------------------------------------------
// The ACTOR side. These are the Tomba G-block offsets other subsystems already name: +0x17E is the
// state-flags word whose bit 9 game/player/actor_tomba.h documents as "paused", and +0x2B is the
// shared object CONTACT byte that ContactStamp (0x80111304) and the pump's contactWeightApply both
// consume. +0x2E/+0x32/+0x36 are the standard integer world X/Y/Z.
namespace actorcontact {
constexpr uint32_t kStateFlags = 382;   // 0x17E — u16
constexpr uint32_t kPausedBit = 512;    // 0x200 — set while the actor is paused
constexpr uint32_t kContactState = 43;  // 0x2B  — u8, the contact/interaction byte
constexpr uint32_t kPosX = 46;          // 0x2E  — u16
constexpr uint32_t kPosY = 50;          // 0x32  — u16
constexpr uint32_t kPosZ = 54;          // 0x36  — u16
constexpr uint32_t kReachXZ = 128;      // 0x80  — s16, horizontal reach for the proximity test
constexpr uint32_t kHeightOffset = 132; // 0x84  — u16, added to the vertical delta
constexpr uint32_t kReachY = 134;       // 0x86  — s16, vertical reach
} // namespace actorcontact

struct ContactActor {
  Core *mCore;
  uint32_t mAt;

  bool paused() const {
    return (mCore->mem_r16(mAt + actorcontact::kStateFlags) & actorcontact::kPausedBit) != 0;
  }
  uint32_t posX() const {
    return mCore->mem_r16(mAt + actorcontact::kPosX);
  }
  uint32_t posY() const {
    return mCore->mem_r16(mAt + actorcontact::kPosY);
  }
  uint32_t posZ() const {
    return mCore->mem_r16(mAt + actorcontact::kPosZ);
  }
  int32_t reachXZ() const {
    return mCore->mem_r16s(mAt + actorcontact::kReachXZ);
  }
  uint32_t heightOffset() const {
    return mCore->mem_r16(mAt + actorcontact::kHeightOffset);
  }
  int32_t reachY() const {
    return mCore->mem_r16s(mAt + actorcontact::kReachY);
  }

  void setContactState(uint8_t v) const {
    mCore->mem_w8(mAt + actorcontact::kContactState, v);
  }
};

// ------------------------------------------------------------------------------------------------
// The OBJECT side. +0x00/+0x03/+0x04/+0x05/+0x06 are the standard node state bytes; +0x62 carries a
// capability mask whose bit 3 gates the proximity test at all; +0xFC links to the record that holds
// the position the test measures against (the object's own body may be elsewhere).
namespace objcontact {
constexpr uint32_t kState = 0;         // u8 — set to 3 on a hit
constexpr uint32_t kKind = 3;          // u8 — must be 1 for the hit reaction to apply
constexpr uint32_t kPhase = 4;         // u8 — must be 1 to react; becomes 2
constexpr uint32_t kSubPhase = 5;      // u8 — cleared on the hit reaction
constexpr uint32_t kTimer = 6;         // u8 — cleared on the hit reaction
constexpr uint32_t kContactState = 43; // 0x2B — u8, stamped with the contact direction
constexpr uint32_t kCapability = 98;   // 0x62 — u16 flags
constexpr uint32_t kProximityBit = 8;  // bit 3 — this object participates in the proximity test
constexpr uint32_t kBodyRecord = 252;  // 0xFC — u32, the record carrying the measured position
} // namespace objcontact

struct ContactObject {
  Core *mCore;
  uint32_t mAt;

  uint32_t kind() const {
    return mCore->mem_r8(mAt + objcontact::kKind);
  }
  uint32_t phase() const {
    return mCore->mem_r8(mAt + objcontact::kPhase);
  }
  bool inProximitySet() const {
    return (mCore->mem_r16(mAt + objcontact::kCapability) & objcontact::kProximityBit) != 0;
  }
  uint32_t bodyRecord() const {
    return mCore->mem_r32(mAt + objcontact::kBodyRecord);
  }

  void setState(uint8_t v) const {
    mCore->mem_w8(mAt + objcontact::kState, v);
  }
  void setPhase(uint8_t v) const {
    mCore->mem_w8(mAt + objcontact::kPhase, v);
  }
  void setSubPhase(uint8_t v) const {
    mCore->mem_w8(mAt + objcontact::kSubPhase, v);
  }
  void setTimer(uint8_t v) const {
    mCore->mem_w8(mAt + objcontact::kTimer, v);
  }
  void setContactState(uint8_t v) const {
    mCore->mem_w8(mAt + objcontact::kContactState, v);
  }
};

// The object's BODY RECORD — the thing whose position the proximity test actually measures against.
// Same +0x2C/+0x30/+0x34 plain-integer world triple AssemblyPartAnchor uses.
struct ContactBody {
  Core *mCore;
  uint32_t mAt;

  uint32_t posX() const {
    return mCore->mem_r16(mAt + 44);
  } // 0x2C
  uint32_t posY() const {
    return mCore->mem_r16(mAt + 48);
  } // 0x30
  uint32_t posZ() const {
    return mCore->mem_r16(mAt + 52);
  } // 0x34
};

class ActorObjectContact {
public:
  // 0x8010E258 — a0 = the actor's G-block, a1 = the object.
  static void resolveHitOrProximity(Core *c);

  static void registerOverrides();
};
