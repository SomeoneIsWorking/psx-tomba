// game/ai/actor_object_contact.cpp — ActorObjectContact::resolveHitOrProximity, guest 0x8010E258 (A00).
//
// WHAT IT DOES, IN GAME TERMS
// Answers two questions about one actor and one object, in order, and does something different for
// each:
//
//   1. DID HE HIT IT? A leading call reports a hit; a non-negative answer means yes. Then this
//      forwards the hit onward and, if the object is the kind that reacts and is currently in its
//      ready phase, knocks it into its struck state: state 3, phase 2, counters cleared, and its
//      contact byte stamped from the published contact direction biased by 2048 — half a turn, i.e.
//      the object is pushed AWAY along the same axis the contact came in on.
//
//   2. IF NOT, IS HE MERELY TOUCHING IT? Only for objects flagged as participating, and never while
//      the actor is paused. It measures against the object's BODY RECORD rather than the object
//      node — a distinction that matters, because the two can be in different places. A horizontal
//      cylinder test (distance vs the actor's reach + 100) and then a vertical band test must both
//      pass; if they do, it computes the contact ANGLE with ratan2, publishes it to the shared
//      scratchpad slot, stamps the ACTOR's contact byte with angle >> 4, and dispatches the follow-up.
//
// The asymmetry is the interesting part: a HIT stamps the OBJECT's contact byte, while mere PROXIMITY
// stamps the ACTOR's. One says "this object was struck from there", the other "the player is leaning
// on something over there".
//
// HOW IT WAS IDENTIFIED
//   * THE SCRATCHPAD SLOT IS ALREADY NAMED. 0x1F80009C is kContactAngleSlot in
//     game/world/collision_resolve.cpp, where cylinderResolve publishes the contact angle for its
//     caller to read back. This function writes the SAME word by the same means (Trig::ratan2 of
//     -dz, dx — the identical argument order), so it is a second producer into an established
//     contract, not a new mechanism.
//   * THE ACTOR FIELDS ARE ALREADY NAMED, INDEPENDENTLY. +0x17E bit 9 is the "paused" gate
//     game/player/actor_tomba.h documents on its own interaction path, and +0x2B is the shared object
//     CONTACT byte that ContactStamp (0x80111304) produces and the pump assembly's contactWeightApply
//     consumes. Both were RE'd earlier and for other purposes.
//   * THE CALLEES PIN THE GEOMETRY. 0x80084080 is Math::sqrtLzc and 0x80085690 is Trig::ratan2, both
//     already natively owned — so the dx²+dz² feeding the first and the (-dz, dx) feeding the second
//     are a horizontal distance and a heading, not arbitrary arithmetic.
//
// WHAT I DID NOT ESTABLISH, and so kept out of the name: what the leading call 0x8010DFD8 actually
// tests (it is unported), what kind of object kind==1 is, and what the follow-ups 0x80022D08 and
// 0x8001FF7C do. The name says what this body decides, which is measured.
//
// TRUE EXTENT: [0x8010E258, 0x8010E400). The single epilogue is L_8010E3E8 (six restores from
// sp+16..sp+36, then sp += 40) and the trailing duplicate `return;` is the recorded binary evidence's dead-tail
// artifact. NOT established from "the next gen function in the shard" — that test is false here (the
// shard split is not address order) and has produced a wrong answer repeatedly this session.
//
// MODULE: defined only by the A00 overlay, so it registers with A00 tomba::native::declareOverride; the main-module
// setter would leave the direct overlay guest 0x8010E258(c) callers on the substrate.
//
// GUEST STACK: frame 40 with SIX spills (r19,r20,ra,r18,r17,r16 at +28,+32,+36,+24,+20,+16 in the
// guest's own program order). All five callee-saved registers stay GuestReg proxies rather than C++
// locals: every one of them is live across at least one of the four nested calls, and a callee
// spilling its caller's callee-saved registers would otherwise write a stale value into guest RAM.
#include "actor_object_contact.h"

#include "core.h"
#include "core/overrides/guest_jal.h"
#include "core/overrides/native_override_catalog.h"
#include "guest_abi.h"

namespace {

// tools/binary ABI evidence 0x8010E258 --scaffold --guestabi, in the guest's program order
constexpr GuestFrameSpill kSpills_8010E258[6] = {
    {19, 28},
    {20, 32},
    {31 /*ra*/, 36},
    {18, 24},
    {17, 20},
    {16, 16},
};

// Return addresses at this function's four jal sites.
constexpr uint32_t kRaAfterHitTest = 0x8010E284u;   // -> overlay guest 0x8010DFD8
constexpr uint32_t kRaAfterDistance = 0x8010E300u;  // -> Math::sqrtLzc   0x80084080
constexpr uint32_t kRaAfterAngle = 0x8010E354u;     // -> Trig::ratan2    0x80085690
constexpr uint32_t kRaAfterProximity = 0x8010E388u; // -> 0x80022D08
constexpr uint32_t kRaAfterHitApply = 0x8010E3A0u;  // -> 0x8001FF7C

constexpr uint32_t kSqrtLzc = 0x80084080u;
constexpr uint32_t kRatan2 = 0x80085690u;
constexpr uint32_t kProximityFollowUp = 0x80022D08u;
constexpr uint32_t kHitFollowUp = 0x8001FF7Cu;

// The shared contact-angle publication slot — the same word collision_resolve.cpp calls
// kContactAngleSlot and writes from cylinderResolve.
constexpr uint32_t kScratchpadBase = 0x1F800000u;
constexpr uint32_t kContactAngleSlot = 156; // 0x1F80009C

// Slack added to each reach before the test, and the shift that turns the published angle into the
// byte the contact field carries.
constexpr int32_t kReachSlackXZ = 100;
constexpr int32_t kHeightSlack = 50;
constexpr int32_t kReachSlackY = 100;
constexpr int32_t kAngleToByte = 4;
// Half a turn (4096 units), added before the shift so a struck object is pushed AWAY from the
// contact rather than toward it.
constexpr int32_t kOppositeTurn = 2048;

// Object state values the hit reaction writes.
constexpr uint8_t kObjKindReacts = 129; // 0x81 — see the delay-slot note at the use site
constexpr uint8_t kObjPhaseReady = 1;
constexpr uint8_t kObjStateStruck = 3;
constexpr uint8_t kObjPhaseStruck = 2;

} // namespace

// ORACLE: overlay guest 0x8010E258
//
// LAYOUT NOTE: the PROXIMITY path is written first and the HIT path last, matching the guest's own
// static order (the guest branches forward to its hit block, which it emits after the proximity
// block). That ordering is not cosmetic — the equivalence gate compares call sites and stores in
// STATIC program order, so writing the hit path first reorders both and fails, which is exactly what
// my first draft did.
void ActorObjectContact::resolveHitOrProximity(Core *c) {
  GuestFrame<40, 6> frame(c, kSpills_8010E258);

  GuestReg<19> actorReg(c);
  actorReg = c->r[4];
  GuestReg<20> objReg(c);
  objReg = c->r[5];
  GuestReg<16> deltaZ(c);
  GuestReg<17> bodyReg(c);
  GuestReg<18> deltaX(c);

  const ContactActor actor{c, c->r[19]};
  const ContactObject obj{c, c->r[20]};

  c->r[6] = 1;
  tomba::guest::dispatchJalToReturn(*c, 0x8010DFD8u, kRaAfterHitTest);
  const int32_t hit = (int32_t)c->r[2];

  if (hit < 0) {
    // ── no hit: is he close enough to count as touching it? ──────────────────────────────────────
    if (actor.paused()) {
      return;
    }
    if (!obj.inProximitySet()) {
      return;
    }

    bodyReg = obj.bodyRecord();
    const ContactBody body{c, c->r[17]};

    deltaX = (uint32_t)(int32_t)(int16_t)(uint16_t)(actor.posX() - body.posX());
    guest_mult(c, (int32_t)(uint32_t)deltaX, (int32_t)(uint32_t)deltaX);
    const uint32_t dxSq = c->lo;
    deltaZ = (uint32_t)(int32_t)(int16_t)(uint16_t)(actor.posZ() - body.posZ());
    guest_mult(c, (int32_t)(uint32_t)deltaZ, (int32_t)(uint32_t)deltaZ);

    c->r[4] = dxSq + c->lo;
    tomba::guest::dispatchJalToReturn(*c, kSqrtLzc, kRaAfterDistance);
    const uint32_t distance = c->r[2] & 0xFFFFu;

    uint32_t touching = 0;
    if (!((actor.reachXZ() + kReachSlackXZ) < (int32_t)distance)) {
      const uint32_t verticalGap =
          (actor.posY() - body.posY() + actor.heightOffset() + (uint32_t)kHeightSlack) & 0xFFFFu;
      if (!((actor.reachY() + kReachSlackY) < (int32_t)verticalGap)) {
        // The heading from the object to the actor, published where the follow-up reads it back.
        c->r[4] = (uint32_t)(0 - (int32_t)(uint32_t)deltaZ);
        c->r[5] = (uint32_t)deltaX;
        tomba::guest::dispatchJalToReturn(*c, kRatan2, kRaAfterAngle);
        c->mem_w32(kScratchpadBase + kContactAngleSlot, c->r[2]);
        touching = 1;
      }
    }
    if (touching == 0) {
      return;
    }

    c->r[4] = c->r[19];
    c->r[5] = c->r[20];
    c->r[6] = 1;
    c->r[7] = 0;
    const uint32_t angle = c->mem_r32(kScratchpadBase + kContactAngleSlot);
    actor.setContactState((uint8_t)((int32_t)angle >> kAngleToByte));
    tomba::guest::dispatchJalToReturn(*c, kProximityFollowUp, kRaAfterProximity);
    return;
  }

  // ── a hit: forward it, then knock a reacting object into its struck state ──────────────────────
  c->r[4] = c->r[19];
  c->r[5] = c->r[20];
  c->r[6] = (uint32_t)hit;
  c->r[7] = 1;
  tomba::guest::dispatchJalToReturn(*c, kHitFollowUp, kRaAfterHitApply);
  // NOTE THE 129, and it is NOT a typo for 1. The guest compares the phase byte against 1, and that
  // branch's delay slot then leaves 129 in the same register, so the SECOND comparison — the kind
  // byte — is against 129. Reading it as 1 would gate the whole hit reaction on the wrong value, and
  // only the delay slot says so.
  if (obj.phase() != kObjPhaseReady) {
    return;
  }
  if (obj.kind() != kObjKindReacts) {
    return;
  }
  obj.setState(kObjStateStruck);
  const uint32_t angle = c->mem_r32(kScratchpadBase + kContactAngleSlot);
  obj.setPhase(kObjPhaseStruck);
  obj.setSubPhase(0);
  obj.setTimer(0);
  // Biased half a turn so the object is driven away from the contact, not into it.
  obj.setContactState((uint8_t)(((int32_t)(angle + (uint32_t)kOppositeTurn)) >> kAngleToByte));
}

void ActorObjectContact::registerOverrides() {
  // A00, from the image: of 23 authenticated MODE images only A00 holds a function at 0x8010E258
  // (108 words), and the address is inside A00's loaded text range.
  tomba::native::declareOverlayOverride(
      "A00", 0x8010E258u, "ActorObjectContact::resolveHitOrProximity", &ActorObjectContact::resolveHitOrProximity);
}
