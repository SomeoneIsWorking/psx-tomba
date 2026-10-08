// game/world/collision_resolve.h — actor-vs-object CYLINDER COLLISION RESOLVE (guest FUN_80023D48).
//
// The busiest unowned function in the game before this port: 29,869 substrate dispatches per 6000
// frames of replays/bugs/seesaw-weight.pad. Full RE — signature, 4-valued return, field map, frame
// contract — in docs/re/collision-resolve-23d48.md.
#pragma once
#include <cstdint>
struct Core;
class Game;

class CollisionResolve {
public:
  // FUN_80023D48(actor = a0, other = a1, anchor = a2, flags = a3) -> v0, where v0 is an OUTCOME
  // rather than a boolean:
  //   0 = no collision (separation test failed, nothing written)
  //   1 = horizontal push-out applied (actor X/Z moved onto the contact circle)
  //   2 = vertical resolve AND the landed flag actor+0x29 set
  //   3 = vertical resolve only
  // flags&1 selects the entry geometry: clear uses the actor's raw position, set offsets the sample
  // point by radius(+0x7C) rotated by angle(+0x56) and carries that offset back out of the final
  // write.
  static void cylinderResolve(Core *c);

  // FUN_8002423C(actor = a0, other = a1) -> v0: 2 = landed (actor Y snapped onto the object's rest
  // height, landed flag at +0x29 set), -1 = rejected by any of the three gates. v0 IS LIVE — overlay
  // a06 tests it against 2 and zeroes a node-scan counter on a hit, so it drives guest writes, not
  // just a register compare. See the implementation banner.
  static void landOnObjectTop(Core *c);

  // FUN_8001F40C(actor = a0, other = a1, suppressSnap = a2) -> v0. The CLASSIFIER of the family:
  // it answers how a body-vs-body contact should be resolved and lets the CALLER do the horizontal
  // push, publishing the contact heading to the shared scratchpad word 0x1F80009C for it to use.
  //   -1 = no contact (either the XZ or the vertical gate rejected; nothing written)
  //    0 = actor is above the other body, XZ is the shorter way out -> caller pushes them apart
  //    1 = actor is above, Y is the shorter way out                 -> caller resolves vertically
  //    2 = actor is level or below, XZ is the shorter way out       -> caller pushes them apart
  //    3 = actor is level or below, Y is the shorter way out        -> UNDERSIDE BUMP: this body
  //        snaps the actor to rest under the other and cancels a rising +0x4A, unless a2 != 0, the
  //        actor is paused (+0x17E & 0x200), or the 0x1F800098 interaction lock is held.
  // Bit 0 is the axis and bit 1 is the side, and callers read them separately — ActorTomba::
  // type8Interact switches on `v0 & 1`, ::stepModeInteract gates on `v0 < 2`. See the .cpp banner
  // for the identification evidence and for the grown-state sibling FUN_8001EC3C.
  static void classifyBodyContact(Core *c);

  // FUN_80023A04(actor = a0, other = a1, policy = a2) -> v0. The OBJECT-vs-OBJECT member of the
  // family: two full object records, no separate anchor, and a POLICY index in a2 that picks which
  // of five vertical-contact rules applies once Y turns out to be the shorter way out.
  //   -1 = no contact, or the chosen policy refused this contact (nothing written)
  //    0 = horizontal push-out applied (actor X/Z moved onto the contact circle)
  //    2 = actor came to rest ON TOP of the other object
  //    3 = actor pushed out UNDERNEATH it
  // NOTE the return codes are NOT cylinderResolve's: there, 0 is the miss and 1 is the push-out.
  // Here 0 IS the push-out and the miss is -1. Unlike cylinderResolve this body also does NOT set
  // the landed flag itself — its caller does, on v0 == 2. See the .cpp banner.
  static void resolveByContactPolicy(Core *c);

  // WHY THIS CANNOT CALL THE Trig METHODS DIRECTLY, even though all five of its callees are owned
  // natively (Math::sqrtLzc, Trig::rcos/rsin/ratan2/angleCmp): Trig::registerOverrides is
  // deliberately an EMPTY body because those substrate bodies descend guest stack frames the native
  // methods do not mirror. Calling trigOf(c).rcos(...) here would leave the callees' frame bytes
  // below this function's sp unwritten while substrate core B writes them, and SBS compares that
  // memory. The body therefore invokes them through their generated func_XXXX wrappers, which is
  // what binary evidence emits and what keeps the guest stack byte-identical.
  static void registerOverrides(Game *game);
};
