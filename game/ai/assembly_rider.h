// game/ai/assembly_rider.h — the ASSEMBLY RIDER: the small object that perches on one arm-end of a
// seaside water-pump assembly, rides it, hops when the pump starts a stroke, and is flung off when
// the pump commands the arm it is sitting on. Plus the typed lenses its per-frame handler needs.
//
// THREE FILES, THREE DIFFERENT OBJECTS — do not confuse them:
//   * game/ai/assembly_node.h    — the ASSEMBLY itself (guest behaviour FUN_8012EB54): the seaside
//                                  water-pump / seesaw, a long diagonal beam with a curved arm and
//                                  two identical hanging end-parts. 12 sub-parts.
//   * game/ai/assembly_companion.h — the jointed actor the assembly SPAWNS and parents to itself
//                                  (handler FUN_80136D9C); it performs a camera routine per stroke.
//   * THIS file                  — a rider that is NOT spawned by the assembly. It is an ordinary
//                                  PLACED field object (resident handler FUN_8004C238 =
//                                  beh_visibility_gate_dispatch, sub-type byte +0x5E == 6) whose
//                                  +0x10 owner link points at an assembly. Its per-frame leaf is the
//                                  area-0 arm of that dispatcher's sub-type-6 case (0x80118B10).
//
// HOW THE RIDER IS IDENTIFIED — MEASURED ON LIVE STATE, not inferred from the neighbourhood.
// Scanning the committed 2 MB RAM dumps for nodes whose +0x1C handler slot is 0x8004C238 finds
// EXACTLY TWO with sub-type +0x5E == 6, identical across every area-0 dump
// (scratch/raw/bucket_f470.bin, c18_free.bin, hud_free.bin, save_prompt.bin):
//
//   node 0x800EDF38  tag(+0x62)=200  slot(+0x18)=3  sceneTag(+0x2A)=10  class(+0x0C)=5
//                    owner(+0x10)=0x800FB858   pos (5727, -1912, 5548)
//   node 0x800EDFC0  tag(+0x62)=204  slot(+0x18)=2  sceneTag(+0x2A)=1   class(+0x0C)=5
//                    owner(+0x10)=0x800FB960   pos (7061, -1767, 3942)
//
// BOTH owners have 0x8012EB54 in their own +0x1C — they ARE the two pump assemblies this project has
// been chasing since kanban #8 (their x positions 5562 / 6678 are the same two nodes
// docs/findings/ai.md matches to the pump x-bands, and 0x800FB960 is the one Tomba's ride/attach
// pointer targets). And the owners' sub-part tables resolve the ride exactly:
//
//   0x800FB858 childPtr(3) = 0x800F308C at (5727, -1757, 5548)   <- rider 0x800EDF38 sits here
//   0x800FB960 childPtr(2) = 0x800F3378 at (7061, -1612, 3942)   <- rider 0x800EDFC0 sits here
//
// X and Z match the rider EXACTLY and Y is ~140 above, which is precisely what the leaf writes. Both
// of those sub-parts carry the SAME model pointer (sceneData 0x801C8F58) while every other slot of
// the assembly carries a different one — i.e. slots 2 and 3 are the assembly's two identical arm-end
// parts, one rider perched on each. The state-0 init's tag->(slot, sceneTag) table reproduces the
// live bytes above exactly (200 -> slot 3 / tag 10, 204 -> slot 2 / tag 1), so the init is confirmed
// to have run and to mean what it reads like.
//
// WHY class 5 MATTERS: Cull::enqueueByClass (game/render/cull.cpp) routes class 5 to CULL QUEUE C,
// and the only submit this leaf performs is Cull::enqueueQueueC (0x80077EFC). The node's own class
// byte and the queue its handler pushes to agree, which is an independent check on the reading.
//
// port_check FOLLOWS THESE LENSES: it harvests one-line `void` write-accessors from game/**/*.h and
// counts a setter as the stores it performs (dynamic differential evidence find_lens_setters). Keep every setter a
// single statement with its mem_wN visible, or the converted body silently stops being gate-able.
#pragma once
#include "assembly_node.h"
#include "core.h"
#include <cstdint>

// The assembly as the RIDER sees it. AssemblyNode already owns the two fields the rider reads to
// decide what to do (modeByte +0x5E, angleSelector +0x6C); the only thing missing is the render
// visibility byte the rider copies, so this view adds exactly that and nothing else.
class RiddenAssembly : public AssemblyNode {
public:
  RiddenAssembly(Core *c, uint32_t at) : AssemblyNode(c, at) {}

  // visible (+0x01, u8): the assembly's own "submitted this frame" byte, written by the shared
  //   visibility gate in beh_visibility_gate_dispatch (game/ai/beh_visibility_gate_dispatch.cpp
  //   state1_gate writes node[1] = 1 on the match path). The rider does not run its own cull test in
  //   its ride state — it INHERITS this byte, so a rider is drawn exactly when the pump it sits on
  //   is, and goes dormant with it.
  uint32_t visible() const {
    return mCore->mem_r8(mAt + 0x01u);
  }
};

// One arm-end sub-part of the assembly, reached through AssemblyNode::childPtr(slot). This is a
// GraphicsBind render record (allocated into owner+0xC0+slot*4 by GraphicsBind::recordArrayInit,
// game/world/graphics_bind.h), and its +0x2C/+0x30/+0x34 hold the part's RESOLVED WORLD POSITION as
// plain 32-bit integers — NOT the 16.16 fixed-point layout an object node uses at the same offsets.
//
// That distinction is load-bearing and is established, not assumed:
//   * ActorReward::resolvePosition (guest FUN_800702C0, game/object/actor_sm_reward.cpp) — already
//     owned — pins an object to one of its owner's linked-entity slots with the IDENTICAL idiom:
//     `obj[+0x2E] = mem_r16(e + 0x2C)`, `obj[+0x32] = mem_r16(e + 0x30) + …`, `obj[+0x36] =
//     mem_r16(e + 0x34)`, and reads the SAME fields as 32-bit when it averages two slots.
//   * GraphicsBind::posCompose (guest FUN_8004BD64) is the engine's generic form of the same
//     operation and reads src[+0x2C/30/34] both ways too.
//   * The leaf in assembly_rider.cpp compares `(y32() - 140)` against the rider's own 16-bit posY.
//     That comparison is only dimensionally sane if this side is a plain integer; against a 16.16
//     value it would be off by a factor of 65536 and the landing test could never fire correctly.
//   * Measured: 0x800F308C reads (5727, -1757, 5548) — real world coordinates, not fixed-point.
class AssemblyPartAnchor {
public:
  AssemblyPartAnchor(Core *c, uint32_t at) : mCore(c), mAt(at) {}
  uint32_t addr() const {
    return mAt;
  }

  // The part's world position truncated to the 16 bits the rider's own position fields hold. The
  // guest uses `lhu` here (a plain 32->16 narrowing of the coordinate), so these stay unsigned.
  uint32_t x16() const {
    return mCore->mem_r16(mAt + 0x2Cu);
  }
  uint32_t y16() const {
    return mCore->mem_r16(mAt + 0x30u);
  }
  uint32_t z16() const {
    return mCore->mem_r16(mAt + 0x34u);
  }
  // The part's world Y at full width — what the rider's landing test compares against.
  int32_t y32() const {
    return (int32_t)mCore->mem_r32(mAt + 0x30u);
  }

private:
  Core *mCore;
  uint32_t mAt;
};

class AssemblyRider {
public:
  AssemblyRider(Core *c, uint32_t at) : mCore(c), mAt(at) {}
  uint32_t addr() const {
    return mAt;
  }

  // owner (+0x10, u32): the assembly this rider is perched on. Verified on live state (see the
  //   banner above) — both riders' +0x10 points at a node whose own handler is 0x8012EB54.
  uint32_t owner() const {
    return mCore->mem_r32(mAt + 0x10u);
  }

  // variantTag (+0x62, s16): the placement-data tag that says WHICH of the two arm-end riders this
  //   is. Only two values occur, 200 and 204, and the state-0 init turns them into the slot index
  //   and scene tag below — 200 -> slot 3 / sceneTag 10, 204 -> slot 2 / sceneTag 1. The mapping is
  //   CROSSED with respect to the numbers (the 200 rider reads owner+0xCC = slot 3), so the tag is
  //   an identifier, not an offset; anything else about it would be a guess. Read SIGNED: the guest
  //   uses `lh` and compares the full 32-bit register against 200/204.
  int32_t variantTag() const {
    return mCore->mem_r16s(mAt + 0x62u);
  }

  // slot (+0x18, u8): which of the assembly's sub-part slots this rider rides — 3 or 2, exactly the
  //   childPtr() index it reads its anchor from. Its OTHER job is to answer the assembly: the fling
  //   only starts when the assembly's angleSelector (+0x6C, "the part being commanded") equals this.
  //   Seeded by the state-0 init from variantTag; measured live as 3 and 2.
  uint32_t slot() const {
    return mCore->mem_r8(mAt + 0x18u);
  }
  void setSlot(uint8_t v) const {
    mCore->mem_w8(mAt + 0x18u, v);
  }

  // alive (+0x00, u8) / visible (+0x01, u8): the standard node pair (see game/object/actor.h). The
  //   init raises alive; the ride state copies the ASSEMBLY's visible byte into this one every frame.
  void setAlive(uint8_t v) const {
    mCore->mem_w8(mAt + 0x00u, v);
  }
  void setVisible(uint8_t v) const {
    mCore->mem_w8(mAt + 0x01u, v);
  }

  // rideState (+0x05, u8): this leaf's own 5-way state machine, dispatched through the jump table at
  //   kRideStateJumpTable. 0 = init, 1 = riding the arm-end, 2 = the hop, 3 = the fling,
  //   4 = dormant. Measured live as 1 on both riders (both pumps at rest).
  uint32_t rideState() const {
    return mCore->mem_r8(mAt + 0x05u);
  }
  void setRideState(uint8_t v) const {
    mCore->mem_w8(mAt + 0x05u, v);
  }

  // motionPhase (+0x06, u8): the step counter WITHIN the hop and the fling. In the hop it is
  //   0 = launch, 1 = first bounce airborne, 2 = second bounce airborne. Both movements zero it when
  //   they are entered.
  uint32_t motionPhase() const {
    return mCore->mem_r8(mAt + 0x06u);
  }
  void setMotionPhase(uint8_t v) const {
    mCore->mem_w8(mAt + 0x06u, v);
  }

  // sceneTag (+0x2A, u8): the per-object scene-mode tag (Actor::setSceneMode's field), seeded per
  //   variant by the init — 10 for the tag-200 rider, 1 for the tag-204 one. NOT read anywhere in
  //   this leaf; it is published for whatever consumes the scene-mode byte. Measured live at exactly
  //   those two values, which is what confirms the init ran.
  void setSceneTag(uint8_t v) const {
    mCore->mem_w8(mAt + 0x2Au, v);
  }

  // contactState (+0x2B, u8): the shared object CONTACT/interaction byte (Actor::interactState).
  //   Value 2 is the "player is bearing on this object" case — the same value the contact producer
  //   FUN_80111304 stamps and that SubstateEdgeLeaves::contactWeightApply turns into the pump's
  //   weight (docs/kanban/cards/008-…). This leaf's tail reads it and, on 2, forces the rider
  //   dormant: touch a rider and it stops riding, hopping or flying.
  uint32_t contactState() const {
    return mCore->mem_r8(mAt + 0x2Bu);
  }

  // Position. posX/posY/posZ (+0x2E/+0x32/+0x36, i16) are the integer world coordinates every other
  //   engine subsystem reads; posYFixed (+0x30, u32) is the 16.16 view whose HIGH halfword IS posY
  //   (docs/findings/object.md, game/object/actor.h) — which is why the hop integrates 32-bit at
  //   +0x30 and then tests 16-bit at +0x32. Same field, two views.
  int32_t posY() const {
    return mCore->mem_r16s(mAt + 0x32u);
  }
  uint32_t posYFixed() const {
    return mCore->mem_r32(mAt + 0x30u);
  }
  void setPosX(uint16_t v) const {
    mCore->mem_w16(mAt + 0x2Eu, v);
  }
  void setPosY(uint16_t v) const {
    mCore->mem_w16(mAt + 0x32u, v);
  }
  void setPosZ(uint16_t v) const {
    mCore->mem_w16(mAt + 0x36u, v);
  }
  void setPosYFixed(uint32_t v) const {
    mCore->mem_w32(mAt + 0x30u, v);
  }

  // velY (+0x4A, i16) / accelY (+0x50, i16): the vertical velocity and gravity the hop integrates,
  //   in 1/256 world units per frame — hence the `<< 8` when velY is folded into the 16.16 position
  //   (the shared arc-motion convention, game/object/actor.h velY/accelY, release_trigger_motion.cpp).
  //   The fling (guest 0x801189B8) reuses the same two fields with its own seeds.
  int32_t velY() const {
    return mCore->mem_r16s(mAt + 0x4Au);
  }
  uint32_t velY_u() const {
    return mCore->mem_r16(mAt + 0x4Au);
  }
  uint32_t accelY_u() const {
    return mCore->mem_r16(mAt + 0x50u);
  }
  void setVelY(uint16_t v) const {
    mCore->mem_w16(mAt + 0x4Au, v);
  }
  void setAccelY(uint16_t v) const {
    mCore->mem_w16(mAt + 0x50u, v);
  }

  // flingSide (+0x47, u8): copied from the assembly's modeByte at the instant the fling starts, and
  //   read by the fling itself (guest 0x801189B8) as the ONLY thing that decides which way the rider
  //   spirals out — mode 2 turns one way (+256 seed, +4 per frame), anything else the other. So the
  //   pump's stroke direction is what throws the rider left or right.
  void setFlingSide(uint8_t v) const {
    mCore->mem_w8(mAt + 0x47u, v);
  }

  // 0x80118B10 — the rider's whole per-frame tick. See the banner in assembly_rider.cpp.
  static void rideSlotAndReactToStroke(Core *c);

  static void registerOverrides();

private:
  Core *mCore;
  uint32_t mAt;
};
