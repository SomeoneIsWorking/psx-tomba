// game/player/actor_targeting.cpp — ActorTargeting::tryAcquireTarget, guest 0x8001FAE0 (MAIN).
//
// WHAT IT DOES, IN GAME TERMS
// Decides whether the actor can ACQUIRE a candidate target — near enough, and in the right direction —
// and if so publishes what the follow-up needs. Four gates, each of which returns outright:
//
//   1. The actor's walk/jump state byte must be 0. He cannot acquire anything mid-action.
//   2. HORIZONTAL REACH: the XZ distance between the two must not exceed the SUM of both bodies'
//      reaches — the same summed-reach convention ActorBump uses when it pushes two bodies apart.
//   3. VERTICAL BAND: their Y separation plus both height offsets must fit inside the summed band.
//   4. AN ANGULAR WINDOW, which is what makes this "acquire" rather than "touch". The heading from the
//      actor to the target is compared against the actor's own heading field, and the difference must
//      land in a 769-unit window offset by 1408 (angles are 4096 per turn). It is deliberately NOT
//      centred on straight-ahead — the window sits to one side — so the acquire is directional in a
//      way a symmetric cone would not be.
//
// On success, and only while the shared contact-kind slot is clear, it publishes the acquired angle
// and a SECOND value to the scratchpad, stamps the target's contact byte, and raises a global — unless
// the actor's guard byte reads 128, which suppresses that last step. The second value is one of two
// constants chosen by the TARGET's type byte (types 14 and 57 get one, everything else the other), so
// the follow-up is told what KIND of thing was acquired as well as where it is.
//
// HOW IT WAS IDENTIFIED
//   * BOTH SIDES ARE READ THROUGH THE SAME OFFSETS — +0x2E/+0x36 position, +0x80 reach, +0x84/+0x86
//     vertical — which is why the lens is shared: actor and target are the same record shape, and the
//     test is symmetric in exactly the way a two-body proximity test should be.
//   * THE FIELDS WERE NAMED ELSEWHERE, INDEPENDENTLY. +0x145 is the walk-state byte
//     CollisionResolve::classifyBodyContact gates on, +0x2B is the shared contact byte, and
//     0x1F800137 is the contact-kind slot classifyBodyContact publishes. This body reads that slot as
//     a precondition, so it sits downstream of the contact pipeline this project already owns.
//   * THE CALLEES PIN THE GEOMETRY: 0x80084080 is Math::sqrtLzc and 0x80085690 is Trig::ratan2, both
//     natively owned, so dx²+dz² into the first and (-dz, dx) into the second is a horizontal distance
//     and a heading — the identical argument order cylinderResolve and ActorObjectContact use.
//
// NOT ESTABLISHED, so kept out of the name and stated plainly rather than guessed:
//   * what the two published constants MEAN (17408 / 18432). They are written to consecutive
//     scratchpad halfwords next to the angle, so they are plainly a parameter for whatever consumes
//     the acquisition, but nothing in the tree names either value.
//   * what target types 14 and 57 are.
//   * what the global at 0x800BF840 = 68 signals, or what guard byte 128 means.
// The name says what this body DECIDES, which is measured, not what the acquisition is FOR.
//
// TRUE EXTENT: [0x8001FAE0, 0x8001FC50). Single epilogue L_8001FC34 (five restores from sp+16..sp+32,
// then sp += 40); the trailing duplicate `return;` is the recorded binary evidence's dead-tail artifact. NOT taken
// from "the next gen function in the shard", which is a false test on this project.
//
// MODULE: MAIN (authenticated executable/overlay evidence guest 0x8001FAE0), so it registers with
// tomba::native::declareOverride — not an overlay setter.
//
// GUEST STACK: frame 40, five spills (r19,ra,r18,r17,r16 at +28,+32,+24,+20,+16 in program order).
// The four callee-saved registers stay GuestReg proxies: r17/r16 carry the deltas across the sqrtLzc
// call and r19/r18 the two bodies across both calls.
#include "actor_targeting.h"

#include "core.h"
#include "core/overrides/guest_jal.h"
#include "core/overrides/native_override_catalog.h"
#include "guest_abi.h"

namespace {

constexpr GuestFrameSpill kSpills_8001FAE0[5] = {
    {19, 28},
    {31 /*ra*/, 32},
    {18, 24},
    {17, 20},
    {16, 16},
};

constexpr uint32_t kRaAfterDistance = 0x8001FB54u;
constexpr uint32_t kRaAfterAngle = 0x8001FBB0u;

constexpr uint32_t kScratchpadBase = 0x1F800000u;
constexpr uint32_t kContactKindSlot = 311; // 0x1F800137 — classifyBodyContact publishes here
constexpr uint32_t kAcquiredAngle = 396;   // 0x1F80018C
constexpr uint32_t kAcquiredParam = 398;   // 0x1F80018E

constexpr uint32_t kAcquireGlobal = 0x800BF840u;
constexpr uint8_t kAcquireSignal = 68;
constexpr uint32_t kGuardSuppresses = 128;

// The angular window. Angles are 4096 per turn; the window is 769 wide, offset by 1408, so it sits to
// one side of straight ahead rather than being centred on it.
constexpr uint32_t kAngleMask = 4095;
constexpr uint32_t kArcOffset = 1408;
constexpr uint32_t kArcWidth = 769;

// Chosen by the TARGET's type byte. What they mean is not established — see the banner.
constexpr uint32_t kTypeA = 14;
constexpr uint32_t kTypeB = 57;
constexpr uint32_t kParamForTypeAB = 17408;
constexpr uint32_t kParamOther = 18432;

} // namespace

// ORACLE: guest 0x8001FAE0
void ActorTargeting::tryAcquireTarget(Core *c) {
  GuestFrame<40, 5> frame(c, kSpills_8001FAE0);

  GuestReg<19> actorReg(c);
  actorReg = c->r[4];
  GuestReg<18> targetReg(c);
  GuestReg<17> deltaX(c);
  GuestReg<16> deltaZ(c);

  const TargetingBody actor{c, c->r[19]};
  targetReg = c->r[5];
  const TargetingBody target{c, c->r[18]};

  if (actor.walkState() != 0) {
    return; // busy — cannot acquire
  }

  deltaX = (uint32_t)(int32_t)(int16_t)(uint16_t)(target.posX() - actor.posX());
  guest_mult(c, (int32_t)(uint32_t)deltaX, (int32_t)(uint32_t)deltaX);
  const uint32_t dxSq = c->lo;
  deltaZ = (uint32_t)(int32_t)(int16_t)(uint16_t)(target.posZ() - actor.posZ());
  guest_mult(c, (int32_t)(uint32_t)deltaZ, (int32_t)(uint32_t)deltaZ);

  c->r[4] = dxSq + c->lo;
  tomba::guest::dispatchJalToReturn(*c, 0x80084080u, kRaAfterDistance);
  const uint32_t distance = c->r[2] & 0xFFFFu;

  if ((target.reach() + actor.reach()) < (int32_t)distance) {
    return; // out of reach
  }

  const uint32_t verticalGap = (actor.posY() - target.posY() + actor.heightOff() + target.heightOff()) & 0xFFFFu;
  if ((actor.bandY() + target.bandY()) < (int32_t)verticalGap) {
    return; // outside the band
  }

  // The heading from the actor to the target, then the directional window.
  c->r[4] = (uint32_t)(0 - (int32_t)(uint32_t)deltaZ);
  c->r[5] = (uint32_t)deltaX;
  tomba::guest::dispatchJalToReturn(*c, 0x80085690u, kRaAfterAngle);
  const uint32_t angle = c->r[2];
  if (!((((angle - actor.heading()) + kArcOffset) & kAngleMask) < kArcWidth)) {
    return;
  }

  // Only while nothing else has already claimed a contact this frame.
  if (c->mem_r8(kScratchpadBase + kContactKindSlot) != 0) {
    return;
  }

  c->mem_w16(kScratchpadBase + kAcquiredAngle, (uint16_t)(angle & kAngleMask));
  const uint32_t targetType = target.typeByte();
  target.setContact(1);
  const uint32_t param = (targetType == kTypeA || targetType == kTypeB) ? kParamForTypeAB : kParamOther;
  c->mem_w16(kScratchpadBase + kAcquiredParam, (uint16_t)param);

  if (actor.guardByte() == kGuardSuppresses) {
    return;
  }
  c->mem_w8(kAcquireGlobal, kAcquireSignal);
}

void ActorTargeting::registerOverrides() {
  tomba::native::declareOverride(0x8001FAE0u, "ActorTargeting::tryAcquireTarget", &ActorTargeting::tryAcquireTarget);
}
