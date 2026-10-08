// game/ai/rope_swing.h — the ROPE/CHAIN swing: a hanging multi-segment rope that swings on a
// spring, decays back to rest, and bends progressively along its length.
//
// See rope_swing.cpp for the identification evidence. This header holds the two typed lenses the
// tick needs — the rope's swing-state block and one segment — plus a named constant per field.
//
// The write accessors are deliberately ONE-LINERS: tools/dynamic differential evidence harvests a lens setter's
// mem_wN width by regex and counts `sw.setSwingVel(v)` as exactly the store it performs, but ONLY
// for lenses defined in a game/**/*.h header whose setters are single statements. A setter that grew
// a second statement would silently stop counting and the gate would compare a short sequence.
#pragma once
#include "core.h"
#include <cstdint>

class Game;

// ------------------------------------------------------------------------------------------------
// The rope's SWING STATE, a block at node+0x60. Everything the swing integrates lives here; the node
// proper only carries the segment count (+0x08) and the segment pointer table (+0xC0).
namespace ropeswing {
constexpr uint32_t kStateBlock = 0x60;

constexpr uint32_t kOutAngle = 0;     // s16 — published angle, copied from kRestAngle each tick
constexpr uint32_t kOutSecond = 2;    // s16 — published companion, kBias + kTargetAngle
constexpr uint32_t kRestAngle = 6;    // s16 — the rope's rest/anchor angle, republished to kOutAngle
constexpr uint32_t kBias = 8;         // s16 — added to kTargetAngle for the second published value
constexpr uint32_t kAngle = 18;       // s16 — the swing ANGLE the segments bend around
constexpr uint32_t kSwingVel = 20;    // s16 — swing VELOCITY: sprung, decayed, clamped, integrated
constexpr uint32_t kImpulse = 26;     // s16 — per-tick push added into the velocity
constexpr uint32_t kTargetAngle = 28; // s16 — the angle the spring pulls back toward
} // namespace ropeswing

struct RopeSwingState {
  Core *mCore;
  uint32_t mAt; // = node + ropeswing::kStateBlock

  uint32_t swingVelRaw() const {
    return mCore->mem_r16(mAt + ropeswing::kSwingVel);
  }
  int32_t swingVel() const {
    return mCore->mem_r16s(mAt + ropeswing::kSwingVel);
  }
  uint32_t impulse() const {
    return mCore->mem_r16(mAt + ropeswing::kImpulse);
  }
  int32_t angle() const {
    return mCore->mem_r16s(mAt + ropeswing::kAngle);
  }
  uint32_t angleRaw() const {
    return mCore->mem_r16(mAt + ropeswing::kAngle);
  }
  int32_t targetAngle() const {
    return mCore->mem_r16s(mAt + ropeswing::kTargetAngle);
  }
  uint32_t targetRaw() const {
    return mCore->mem_r16(mAt + ropeswing::kTargetAngle);
  }
  uint32_t restAngle() const {
    return mCore->mem_r16(mAt + ropeswing::kRestAngle);
  }
  uint32_t bias() const {
    return mCore->mem_r16(mAt + ropeswing::kBias);
  }

  void setSwingVel(uint16_t v) const {
    mCore->mem_w16(mAt + ropeswing::kSwingVel, v);
  }
  void setAngle(uint16_t v) const {
    mCore->mem_w16(mAt + ropeswing::kAngle, v);
  }
  void setOutAngle(uint16_t v) const {
    mCore->mem_w16(mAt + ropeswing::kOutAngle, v);
  }
  void setOutSecond(uint16_t v) const {
    mCore->mem_w16(mAt + ropeswing::kOutSecond, v);
  }
};

// ------------------------------------------------------------------------------------------------
// The rope NODE and one SEGMENT. node+0xC0 is the same sub-part pointer table AssemblyNode::childPtr
// and NodeXform::propagate walk; a segment's +0x0C is what node_xform.cpp's CHILD-role lens calls
// childEulerZ (game/render/node_xform.cpp:89) — the segment's own Z rotation, which is exactly the
// axis a hanging rope bends around.
namespace ropenode {
constexpr uint32_t kSegmentCount = 0x08; // u8  — how many segments this rope has
constexpr uint32_t kSegmentTable = 0xC0; // u32[] — one pointer per segment
constexpr uint32_t kSegmentBendZ = 0x0C; // s16 on a SEGMENT — its Z rotation (childEulerZ)
} // namespace ropenode

struct RopeNode {
  Core *mCore;
  uint32_t mAt;

  uint32_t segmentCount() const {
    return mCore->mem_r8(mAt + ropenode::kSegmentCount);
  }
  uint32_t segmentPtr(uint32_t i) const {
    return mCore->mem_r32(mAt + ropenode::kSegmentTable + i * 4u);
  }
};

struct RopeSegment {
  Core *mCore;
  uint32_t mAt;

  void setBendZ(uint16_t v) const {
    mCore->mem_w16(mAt + ropenode::kSegmentBendZ, v);
  }
};

class RopeSwing {
public:
  // 0x801281B8 — the rope's whole per-frame tick. a0 = the rope node.
  static void swingTickAndBendSegments(Core *c);

  static void registerOverrides();
};
