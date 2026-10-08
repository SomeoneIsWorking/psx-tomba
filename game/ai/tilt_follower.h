// game/ai/tilt_follower.h — the TILT FOLLOWER: an object that pitches at HALF the tilt of one of
// its owner's sub-parts, so it leans with a moving part instead of staying rigid.
//
// See tilt_follower.cpp for the identification evidence. This header holds only the two typed lenses
// the leaf needs, plus a named constant per field, so the body reads as "take the owner's arm pitch,
// wrap it, halve it, become that" rather than as a chain of mem_r16(base + 8).
//
// The write accessor is deliberately a ONE-LINER: tools/dynamic differential evidence harvests a lens setter's mem_wN
// width by regex and counts `self.setPitch(v)` as exactly the store it performs, but ONLY for lenses
// defined in a game/**/*.h header whose setters are single statements. A setter that grew a second
// statement or a nested brace would silently stop counting, and the gate would compare a short
// sequence (see docs/findings/tooling.md).
#pragma once
#include "core.h"
#include <cstdint>

// ------------------------------------------------------------------------------------------------
// The follower node itself. Same standard object-node shape the rest of game/ai uses — see
// game/object/actor.h, whose Actor lens names +0x54 rotX and +0x2B the contact/interaction byte.
namespace tiltfollow {
constexpr uint32_t kOwner = 0x10;        // u32 — the node this one is attached to
constexpr uint32_t kContactState = 0x2B; // u8  — the shared object CONTACT byte (Actor::interactState)
constexpr uint32_t kRotX = 0x54;         // s16 — the node's X rotation (pitch); Actor::rotX
} // namespace tiltfollow

struct TiltFollowerNode {
  Core *mCore;
  uint32_t mAt;

  uint32_t owner() const {
    return mCore->mem_r32(mAt + tiltfollow::kOwner);
  }
  uint32_t contactState() const {
    return mCore->mem_r8(mAt + tiltfollow::kContactState);
  }

  void setPitch(uint16_t v) const {
    mCore->mem_w16(mAt + tiltfollow::kRotX, v);
  }
};

// ------------------------------------------------------------------------------------------------
// One sub-part of the owner, reached through the owner's +0xC0 pointer table — the SAME table
// AssemblyNode::childPtr and NodeXform::propagate walk (game/ai/assembly_node.h,
// game/render/node_xform.cpp). Only the part's own X Euler angle is needed here.
namespace tiltpart {
constexpr uint32_t kChildTable = 0xC0;  // owner+0xC0: array of sub-part pointers, one per slot
constexpr uint32_t kChildEulerX = 0x08; // s16 — the sub-part's X Euler angle, node_xform's childEulerX
} // namespace tiltpart

struct TiltSourcePart {
  Core *mCore;
  uint32_t mAt;

  // Read SIGNED: the guest uses `lh`, and the wrap below depends on the sign already being correct.
  int32_t childEulerX() const {
    return mCore->mem_r16s(mAt + tiltpart::kChildEulerX);
  }
};

class Game;

class TiltFollower {
public:
  // 0x80125FE0 — substate 0 of outer state 1 of behaviour FUN_80125E0C. a0 = the follower node.
  static void applyHalvedOwnerPartPitch(Core *c);

  static void registerOverrides();
};
