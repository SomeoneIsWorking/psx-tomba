// game/ai/rope_swing.cpp — RopeSwing::swingTickAndBendSegments, guest 0x801281B8 (A00).
//
// WHAT IT DOES, IN GAME TERMS
// A hanging rope (or chain) built from N linked segments swings on a spring and bends progressively
// along its length. One tick is four stages:
//
//   1. SPRING. The per-tick impulse is added into the swing velocity. If the rope's current angle has
//      overshot the angle the spring pulls toward, six times the overshoot is subtracted back out —
//      a restoring force proportional to how far it is past rest.
//   2. DECAY TO REST. The velocity is pulled 100 toward zero and CLAMPED AT zero rather than allowed
//      to cross it, so a swinging rope settles instead of oscillating forever. The two arms are
//      mirror images (subtract 100 when positive, add 100 when negative), each snapping to exactly 0
//      if that step would change the sign.
//   3. SPEED LIMIT. The velocity is clamped to +/-15360.
//   4. INTEGRATE AND BEND. The angle advances by velocity/256, then the bend is distributed down the
//      rope: segment i gets `(bendUnit * (i + 2)) >> 4` minus a constant `bendUnit >> 5`, where
//      bendUnit = (angle * 5 >> 4) + 25. The multiply by (i+2) is what makes the bend GROW along the
//      chain — the tip swings further than the segment nearest the anchor, which is what a rope does.
//
// HOW IT WAS IDENTIFIED
//   * THE RENDER HANDLER NAMES THE OBJECT. The node's own render slot obj[+0x18] holds
//     0x8013E9D8, the rope-anchor render fn, i.e. the engine draws this node as a rope anchor. That
//     is what fixes "hanging multi-segment thing" as a rope rather than a generic jointed prop.
//   * THE SEGMENT FIELD WAS ALREADY NAMED, INDEPENDENTLY. node+0xC0 is the sub-part pointer table
//     that AssemblyNode::childPtr and NodeXform::propagate both walk, and +0x0C on a sub-part is
//     childEulerZ in game/render/node_xform.cpp's CHILD-role lens (line 89) — RE'd earlier and for an
//     unrelated purpose. Z is precisely the axis a rope hanging in a vertical plane bends around, so
//     the write target agrees with the physical reading rather than merely being consistent with it.
//   * THE SHAPE OF THE LOOP CONFIRMS IT. Writing an angle that scales with the segment INDEX is the
//     signature of a chain, not of a rigid assembly: a rigid part would take one shared transform.
//
// WHAT I DID NOT ESTABLISH, and so did not put in the name: which rope this is, or what supplies the
// per-tick impulse at +0x1A (wind, the player grabbing it, a swinging weight). The callee at
// 0x801284AC runs first each tick and is the obvious candidate, but it is unported and I did not RE
// it. The name says what the tick DOES, which is measured.
//
// TRUE EXTENT: [0x801281B8, 0x80128300). The body's single epilogue is L_801282F8 (restore ra from
// sp+20 and s0 from sp+16, sp += 24) and the recorded binary evidence's trailing duplicate `return;` after it is
// its usual dead-tail artifact. Deliberately NOT established from "the next gen function in the
// shard" — that test is false on this project (the shard split is not address order) and has given a
// wrong answer four times this session.
//
// MODULE: defined only by the A00 overlay (`overlay guest 0x801281B8`, authenticated executable/overlay evidence), so
// it registers with A00 tomba::native::declareOverride; the main-module setter would leave the direct `overlay guest
// 0x801281B8(c)` callers on the substrate.
//
// GUEST STACK: frame 24, spills s0 at +16 and ra at +20. s0 holds the node and is live across the
// leading call, so it is a GuestReg proxy and not a C++ local — a callee spilling its caller's
// callee-saved registers would otherwise write a stale value into guest RAM.
//
// FIDELITY NOTE: the guest stores the swing velocity MANY times per tick — once per stage, including
// on both arms of every branch — rather than computing it in a register and storing once. Those
// intermediate stores are guest-visible state that SBS compares, so the structure below preserves
// them exactly instead of collapsing the arithmetic.
#include "rope_swing.h"

#include "core.h"
#include "core/overrides/guest_jal.h"
#include "core/overrides/native_override_catalog.h"
#include "guest_abi.h"

namespace {

// tools/binary ABI evidence 0x801281B8 --scaffold --guestabi, program order
constexpr GuestFrameSpill kSpills_801281B8[2] = {
    {16, 16},
    {31 /*ra*/, 20},
};

constexpr uint32_t kRaAfterPreTick = 0x801281CCu; // -> overlay guest 0x801284AC, runs before the swing

// Stage 1: how hard the spring pulls back per unit of overshoot.
constexpr int32_t kRestoreGain = 6;
// Stage 2: how much velocity bleeds off per tick, snapping to zero rather than crossing it.
constexpr int32_t kDecayPerTick = 100;
// Stage 3: the swing speed limit.
constexpr int32_t kSwingSpeedLimit = 15360;
// Stage 4: angle -> bend. bendUnit = (angle * 5 >> 4) + 25; segment i gets (bendUnit*(i+2) >> 4)
// minus the shared (bendUnit >> 5).
constexpr int32_t kBendNumer = 5;
constexpr int32_t kBendShift = 4;
constexpr int32_t kBendBase = 25;
constexpr int32_t kSegmentShift = 4;
constexpr int32_t kBendTrimShift = 5;
constexpr int32_t kVelToAngleShift = 8; // angle += velocity / 256

} // namespace

// ORACLE: overlay guest 0x801281B8
void RopeSwing::swingTickAndBendSegments(Core *c) {
  GuestFrame<24, 2> frame(c, kSpills_801281B8);

  GuestReg<16> nodeReg(c); // s0 — live across the call below
  nodeReg = c->r[4];
  tomba::guest::dispatchJalToReturn(*c, 0x801284ACu, kRaAfterPreTick);

  const RopeNode node{c, c->r[16]};
  const RopeSwingState sw{c, c->r[16] + ropeswing::kStateBlock};

  // ── 1. spring: add the impulse, then pull back six times any overshoot past the target ─────────
  const uint32_t pushed = sw.swingVelRaw() + sw.impulse();
  const int32_t angle0 = sw.angle();
  const int32_t target = sw.targetAngle();
  sw.setSwingVel((uint16_t)pushed); // stored on BOTH arms (guest delay slot)
  if (target < angle0) {
    const int32_t overshoot = angle0 - target;
    sw.setSwingVel((uint16_t)(pushed - (uint32_t)(overshoot * kRestoreGain)));
  }

  // ── 2. decay toward rest, snapping to exactly zero rather than crossing it ─────────────────────
  if (sw.swingVel() >= 0) {
    const uint32_t dec = sw.swingVelRaw() - (uint32_t)kDecayPerTick;
    sw.setSwingVel((uint16_t)dec);
    if ((int32_t)(dec << 16) < 0) {
      sw.setSwingVel(0); // went negative -> rest
    }
  } else {
    const uint32_t inc = sw.swingVelRaw() + (uint32_t)kDecayPerTick;
    sw.setSwingVel((uint16_t)inc);
    if ((int32_t)(inc << 16) > 0) {
      sw.setSwingVel(0); // went positive -> rest
    }
  }

  // ── 3. speed limit ─────────────────────────────────────────────────────────────────────────────
  // ONE store site, as the guest has: both out-of-range arms fall into the same store and the
  // in-range case skips it entirely. Writing a store per arm would be byte-identical in effect but
  // adds a static store the oracle does not have, which the equivalence gate compares.
  {
    const int32_t v = sw.swingVel();
    int32_t limited = 0;
    bool outOfRange = true;
    if (v < -kSwingSpeedLimit) {
      limited = -kSwingSpeedLimit;
    } else if (v < kSwingSpeedLimit + 1) {
      outOfRange = false;
    } else {
      limited = kSwingSpeedLimit;
    }
    if (outOfRange) {
      sw.setSwingVel((uint16_t)(uint32_t)limited);
    }
  }

  // ── 4. integrate the angle, republish, and bend the segments ───────────────────────────────────
  const int32_t velStep = (int32_t)(sw.swingVelRaw() << 16) >> (16 + kVelToAngleShift);
  const uint32_t rest = sw.restAngle();
  sw.setAngle((uint16_t)(sw.angleRaw() + (uint32_t)velStep));
  sw.setOutAngle((uint16_t)rest);
  sw.setOutSecond((uint16_t)(sw.bias() + sw.targetRaw()));

  const int32_t bendUnit = ((sw.angle() * kBendNumer) >> kBendShift) + kBendBase;
  const uint32_t count = node.segmentCount();
  if (count == 0) {
    return;
  }

  // The bend GROWS with the segment index — the tip travels further than the anchor end.
  const int32_t trim = bendUnit >> kBendTrimShift;
  for (uint32_t i = 0; i < count; i++) {
    guest_mult(c, bendUnit, (int32_t)(i + 2u));
    const RopeSegment seg{c, node.segmentPtr(i)};
    seg.setBendZ((uint16_t)(((int32_t)c->lo >> kSegmentShift) - trim));
    if (!((int32_t)(i + 1u) < (int32_t)node.segmentCount())) {
      break;
    }
  }
}

void RopeSwing::registerOverrides() {
  // A00, from the image: of 23 authenticated MODE images only A00 holds a function at 0x801281B8
  // (84 words), and the address is inside A00's loaded text range. This is the MODULE note above,
  // applied.
  tomba::native::declareOverlayOverride(
      "A00", 0x801281B8u, "RopeSwing::swingTickAndBendSegments", &RopeSwing::swingTickAndBendSegments);
}
