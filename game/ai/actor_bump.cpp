// game/ai/actor_bump.cpp — ActorBump::respondToContact, guest 0x8010EA80 (A00).
//
// WHAT IT DOES, IN GAME TERMS
// The actor has run into something. This decides which of three things happens, and the branch order
// is the design: INTERACT if the situation allows it, otherwise PUSH APART, and if the other thing
// was not in its receptive state, RECOIL instead.
//
//   * Nothing at all while the actor is paused, or while a global gate byte is set.
//   * It first asks CollisionResolve::classifyBodyContact (0x8001F40C, already ported) what kind of
//     contact this is. A negative answer means no contact and the body returns.
//   * INTERACT — only when the other object is in state 1, the actor's walk state is 1, and the
//     contact class is under 2. Flag bit 15 picks between a special interaction and a generic one.
//   * PUSH APART — the same situation but any of those conditions fails: place the actor on the
//     contact circle around the other object, at cos/sin of the published contact angle scaled by the
//     SUM of both bodies' reaches. X is added and Z is SUBTRACTED, which is the same handedness
//     collision_resolve.cpp's push-out uses.
//   * RECOIL — reached when the other object was NOT in state 1. Gated on the contact-kind byte being
//     clear, the actor not already busy, and his walk state under 2. The actor is stamped with the
//     contact direction, knocked to state 3 / phase 2 / sub-phase 2, and given a 120-tick recoil
//     timer. Whether the direction is biased half a turn depends on Trig::angleCmp comparing his
//     FACING against the contact angle — i.e. being hit from behind reads differently from head-on.
//
// HOW IT WAS IDENTIFIED
//   * IT CONSUMES TWO CONTRACTS THIS PROJECT ALREADY OWNS. 0x8001F40C is
//     CollisionResolve::classifyBodyContact, ported earlier today, and 0x1F80009C is the contact-angle
//     slot that both cylinderResolve and ActorObjectContact publish. So this is the CONSUMER end of
//     the contact pipeline whose producers were already named — it is not a new mechanism.
//   * THE MATH CALLEES PIN THE GEOMETRY. 0x80083F50 / 0x80083E80 are Trig::rcos / Trig::rsin and
//     0x80077768 is Trig::angleCmp, all natively owned, so cos/sin of the published angle scaled by
//     summed reaches is a push onto a contact circle, and the angleCmp is a facing test.
//   * THE ACTOR FIELDS WERE NAMED ELSEWHERE, INDEPENDENTLY: +0x17E bit 9 "paused" and +0x144 walk
//     state from actor_tomba.h / classifyBodyContact, +0x2B the shared contact byte.
//
// NOT ESTABLISHED, so kept out of the name: what the special-interaction callees 0x8001F830 and
// 0x8001FDB4 do (both unported), and what the gate byte at 0x800BF841 means.
//
// LAYOUT NOTE: the branches are written in the guest's own STATIC order — interact, then push-apart,
// then recoil — because the equivalence gate compares call sites and stores in static program order.
// Writing them in any other order fails on both axes at once; that mistake cost a re-run on the
// previous port in this session.
//
// TRUE EXTENT: [0x8010EA80, 0x8010EC58). Single epilogue L_8010EC40 (four restores from sp+16..sp+28,
// then sp += 32); the trailing duplicate `return;` is the recorded binary evidence's dead-tail artifact. NOT taken
// from "the next gen function in the shard", which is a false test here.
//
// MODULE: A00 only, so A00 tomba::native::declareOverride.
//
// GUEST STACK: frame 32, spills r17,ra,r18,r16 at +20,+28,+24,+16 in the guest's program order. All
// three callee-saved registers stay GuestReg proxies — each is live across at least one nested call.
#include "actor_bump.h"

#include "core.h"
#include "core/overrides/guest_jal.h"
#include "core/overrides/native_override_catalog.h"
#include "guest_abi.h"

namespace {

constexpr GuestFrameSpill kSpills_8010EA80[4] = {
    {17, 20},
    {31 /*ra*/, 28},
    {18, 24},
    {16, 16},
};

constexpr uint32_t kRaAfterClassify = 0x8010EAC8u;
constexpr uint32_t kRaAfterSpecial = 0x8010EB20u;
constexpr uint32_t kRaAfterGeneric = 0x8010EB3Cu;
constexpr uint32_t kRaAfterCos = 0x8010EB50u;
constexpr uint32_t kRaAfterSin = 0x8010EB74u;
constexpr uint32_t kRaAfterAngleCmp = 0x8010EBF8u;

constexpr uint32_t kClassifyBodyContact = 0x8001F40Cu; // CollisionResolve::classifyBodyContact
constexpr uint32_t kSpecialInteract = 0x8001F830u;
constexpr uint32_t kGenericInteract = 0x8001FDB4u;
constexpr uint32_t kRcos = 0x80083F50u;     // Trig::rcos
constexpr uint32_t kRsin = 0x80083E80u;     // Trig::rsin
constexpr uint32_t kAngleCmp = 0x80077768u; // Trig::angleCmp

constexpr uint32_t kScratchpadBase = 0x1F800000u;
constexpr uint32_t kContactAngleSlot = 156; // 0x1F80009C — shared with cylinderResolve
constexpr uint32_t kContactKindSlot = 311;  // 0x1F800137 — classifyBodyContact publishes here
constexpr uint32_t kContactFlagSlot = 386;  // 0x1F800182 — cleared once a contact is accepted

constexpr uint32_t kGlobalGate = 0x800BF841u; // non-zero suppresses the whole response

constexpr int32_t kTrigShift = 12; // trig terms are 12-bit fixed
constexpr int32_t kAngleToByte = 4;
constexpr int32_t kOppositeTurn = 2048; // half a turn
constexpr uint8_t kOtherReceptive = 1;
constexpr uint8_t kWalkReady = 1;
constexpr int32_t kContactClassLimit = 2;
constexpr uint8_t kRecoilState = 3;
constexpr uint8_t kRecoilPhase = 2;
constexpr uint16_t kRecoilTicks = 120;

} // namespace

// ORACLE: overlay guest 0x8010EA80
void ActorBump::respondToContact(Core *c) {
  GuestFrame<32, 4> frame(c, kSpills_8010EA80);

  GuestReg<17> actorReg(c);
  actorReg = c->r[4];
  GuestReg<18> otherReg(c);
  GuestReg<16> scratch(c);

  const BumpActor actor{c, c->r[17]};
  otherReg = c->r[5];
  const BumpOther other{c, c->r[18]};

  if (actor.paused()) {
    return;
  }
  if (c->mem_r8(kGlobalGate) != 0) {
    return;
  }

  c->r[6] = 1;
  tomba::guest::dispatchJalToReturn(*c, kClassifyBodyContact, kRaAfterClassify);
  const int32_t contactClass = (int32_t)c->r[2];
  if (contactClass < 0) {
    return;
  }

  c->mem_w8(kScratchpadBase + kContactFlagSlot, 0);

  if (other.state() == kOtherReceptive) {
    const bool ready = actor.walkState() == kOtherReceptive && contactClass < kContactClassLimit;
    if (ready) {
      // ── INTERACT ─────────────────────────────────────────────────────────────────────────────
      if ((actor.stateFlags() & (int32_t)actorbump::kSpecialBit) != 0) {
        c->r[4] = c->r[17];
        c->r[5] = c->r[18];
        tomba::guest::dispatchJalToReturn(*c, kSpecialInteract, kRaAfterSpecial);
        return;
      }
      c->r[4] = c->r[18];
      c->r[5] = (uint32_t)-32766;
      c->r[6] = 3;
      c->r[7] = 32;
      tomba::guest::dispatchJalToReturn(*c, kGenericInteract, kRaAfterGeneric);
      return;
    }

    // ── PUSH APART: put the actor on the contact circle around the other body ──────────────────
    scratch = kScratchpadBase;
    c->r[4] = c->mem_r32(scratch + kContactAngleSlot);
    tomba::guest::dispatchJalToReturn(*c, kRcos, kRaAfterCos);
    guest_mult(c, (int32_t)c->r[2], actor.reach() + other.reach());
    c->r[4] = c->mem_r32(scratch + kContactAngleSlot);
    const int32_t offsetX = (int32_t)c->lo >> kTrigShift;
    scratch = (uint32_t)offsetX; // s0 carries it across the rsin call
    tomba::guest::dispatchJalToReturn(*c, kRsin, kRaAfterSin);
    guest_mult(c, (int32_t)c->r[2], actor.reach() + other.reach());
    actor.setPosX((uint16_t)(other.posX() + (uint32_t)scratch));
    const int32_t offsetZ = (int32_t)c->lo >> kTrigShift;
    actor.setPosZ((uint16_t)(other.posZ() - (uint32_t)offsetZ));
    return;
  }

  // ── RECOIL: the other object was not receptive, so the actor takes the hit ───────────────────
  if (c->mem_r8(kScratchpadBase + kContactKindSlot) != 0) {
    return;
  }
  if ((actor.state() & actorbump::kStateBusyMask) != 0) {
    return;
  }
  if (!(actor.walkState() < 2u)) {
    return;
  }

  scratch = kScratchpadBase;
  c->r[4] = (uint32_t)actor.facing();
  c->r[5] = (uint32_t)c->mem_r16s(scratch + kContactAngleSlot);
  c->r[6] = 0;
  tomba::guest::dispatchJalToReturn(*c, kAngleCmp, kRaAfterAngleCmp);
  // Hit from one side reads the angle straight; from the other it is biased half a turn, so the
  // recoil always drives him away from whatever struck him.
  const uint32_t angle = c->mem_r32(scratch + kContactAngleSlot);
  const int32_t stamped =
      (c->r[2] != 0) ? ((int32_t)angle >> kAngleToByte) : ((int32_t)(angle + (uint32_t)kOppositeTurn) >> kAngleToByte);
  actor.setContact((uint8_t)stamped);
  actor.setPhase(kRecoilPhase);
  actor.setSubPhase(kRecoilPhase);
  actor.setState(kRecoilState);
  actor.setTimer(0);
  actor.setRecoilTimer(kRecoilTicks);
}

void ActorBump::registerOverrides() {
  // A00, from the image: of 23 authenticated MODE images only A00 holds a function at 0x8010EA80
  // (118 words), and the address is inside A00's loaded text range.
  tomba::native::declareOverlayOverride(
      "A00", 0x8010EA80u, "ActorBump::respondToContact", &ActorBump::respondToContact);
}
