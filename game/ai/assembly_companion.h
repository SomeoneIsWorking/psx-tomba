// game/ai/assembly_companion.h — the ASSEMBLY COMPANION: the jointed actor that a field assembly
// spawns and parents to itself, plus the natively-owned leaf of its idle sub-state (guest 0x80138A64).
//
// The name is not new. `SubstateEdgeLeaves::spawnInnerDispatchChild` (game/ai/substate_edge_native.cpp,
// guest 0x8013892C) already calls this node "the assembly's COMPANION node"; this file follows that
// vocabulary rather than inventing a second word for the same object.
//
// NOT the same thing as game/ai/assembly_node.h. That file is the lens over the ASSEMBLY itself
// (guest behaviour FUN_8012EB54 — the seaside water-pump/seesaw class: a swinging beam with an arm,
// a bucket and a counterweight) and over the sub-part records the assembly animates. THIS file is the
// lens over the separate node the assembly SPAWNS at its own position, whose own behaviour handler is
// FUN_80136D9C. The two are linked parent->child, and the leaf below reads across that link.
#pragma once
#include "core.h"
#include <cstdint>

// The global camera vertical-look MODE byte. It is not this companion's own state — it is a shared,
// one-at-a-time channel into the field camera, and this family is one of its only two writers in the
// A00 overlay (the other is its own sibling leaf, guest 0x80137198). The guest addresses it as
// `lui v0, 0x800C` + a signed -0x7DF displacement, which is why it reads as `+ (uint32_t)-2015` in
// authenticated executable/overlay evidence — grep either form and land here.
//
// WHAT READS IT, and therefore what it MEANS: CutsceneCamera::pitch (guest FUN_8006D654,
// game/camera/cutscene_camera.cpp) switches its height target on this byte in the default arm of its
// selector — 0 = the ordinary follow height, 1 = the target pulled 200 units DOWN, anything else =
// the raised/offset variants. Two more A00 functions read it as a plain "is a mode held" flag:
// overlay guest 0x8010DBE4 and overlay guest 0x8010CF90 both force a per-node halfword at +0x26 to -600 while
// it is non-zero instead of deriving it from the clock at scratchpad 0x1F800160. (What that +0x26
// field then FEEDS is not established — do not build on it. Neither of those two functions is
// natively owned yet.)
//
// It is UNDOCUMENTED ANYWHERE ELSE in this repo: it has zero hits in docs/, in the findings registry
// and in the journal, and although it lives at +0x19 of the 104-byte control block at 0x800BF808
// (Pool::resetControlBlock, game/world/pool.cpp), that reset only seeds +7/+40/+41, so +0x19 is
// merely zeroed at area reset and never named. This comment is the first record of it.
constexpr uint32_t kCamPitchMode = 0x800BF821u;
// The value 0 this family writes to release it. It never writes a literal non-zero: the claim path
// COPIES the parent assembly's stroke-phase code into this byte, which is why the two share one
// 0/1/2 domain. See endCamHoldAndRearmOnStroke.
constexpr uint8_t kCamPitchNormal = 0;

class AssemblyCompanion {
public:
  AssemblyCompanion(Core *c, uint32_t at) : mCore(c), mAt(at) {}
  uint32_t addr() const {
    return mAt;
  }

  // --- how this node comes into existence, which is where every field meaning below comes from ----
  // The assembly calls SubstateEdgeLeaves::spawnInnerDispatchChild (0x8013892C) once its role byte is
  // < 3 — from its state-0 init (overlay guest 0x8012ED84 ← FUN_8012EB54) and again from the OPN-overlay
  // assembly hook 0x8018C820. That spawner goes through Placement::spawnWithParent
  // (game/world/placement.cpp, guest FUN_80072DDC) with (parent, 3, 4, 10), and THAT is what writes
  // `newNode+0x10 = parent` — the store making parent() below a fact rather than a guess. The spawner
  // then installs 0x80136D9C as the new node's behaviour handler at +0x1C, copies the assembly's
  // world position, and sets the child's role byte from the parent's.

  // parent (+0x10, u32): the ASSEMBLY node that spawned this companion. Read it through AssemblyNode.
  uint32_t parent() const {
    return mCore->mem_r32(mAt + 0x10u);
  }

  // role (+0x03, u8): copied from the parent assembly's own role byte at spawn, EXCEPT that a role-0
  //   parent yields role 3 while the scripted-sequence mode byte 0x800BF89C == 2. That fork is what
  //   the outer dispatcher (beh_pure_inner_dispatch, FUN_80136D9C) tests: role 3 routes its idle
  //   sub-state to a different overlay's leaf (0x8018CDC4) instead of the leaf in this file. So by
  //   construction the leaf here only ever sees roles 0..2 — and in every committed 2 MB RAM dump the
  //   three live companions carry roles 0, 1 and 2, so the leaf here is the arm that actually runs.
  //   Role 0 additionally suppresses both of this leaf's camera-mode writes: it re-arms silently.
  //   The role also selects the companion's own rig: FUN_80136F08 (its state-0 init) indexes a
  //   part-count byte table at 0x8014A9A4 and a blueprint-pointer table at 0x8014A994 by this byte,
  //   giving 6, 9, 10 and 9 sub-parts for roles 0..3 (roles 1 and 3 share the blueprint at
  //   0x8014A888), each blueprint a list of per-part local offsets, Euler angles and a part id.
  uint8_t role() const {
    return mCore->mem_r8(mAt + 0x03u);
  }

  // subState (+0x05, u8) / step (+0x06, u8): the two-level sub-state the dispatcher walks. subState
  //   0 = idle (this leaf), 1 = the performance (guest 0x80137198), 2 = 0x8018CA1C. `step` is the
  //   performance's own step counter; the performance zeroes both when it ends, which is what hands
  //   control back to this leaf with the camera mode still held.
  uint8_t subState() const {
    return mCore->mem_r8(mAt + 0x05u);
  }
  void setSubState(uint8_t v) const {
    mCore->mem_w8(mAt + 0x05u, v);
  }
  void setStep(uint8_t v) const {
    mCore->mem_w8(mAt + 0x06u, v);
  }

  // --- the companion's own RIG: a table of sub-part records it owns and poses every visible frame -
  // partCount (+0x08, u8): how many sub-parts the rig has. Written by the state-0 init
  //   overlay guest 0x80136F08 as `node[8] = *(u8*)(0x8014A9A4 + role)` — the per-role part-count table
  //   already described on role() above (6, 9, 10, 9 for roles 0..3) — and mirrored into node[9]
  //   immediately after. game/render/node_xform.cpp's Node lens calls the same pair
  //   childCount(0x08)/childCountGuard(0x09), which is what NodeXform's transform loops bound on.
  uint8_t partCount() const {
    return mCore->mem_r8(mAt + 0x08u);
  }
  // kPartTable (+0xC0): the sub-part POINTER array, one u32 per part, in the standard scene-node
  //   place. The same init loop fills it: for each part it calls the generic child spawner
  //   0x8007AAE8 and stores the result into node+0xC0+4*i. Read each entry through AssemblyChild
  //   (game/ai/assembly_node.h) — it is the same CHILD-role record type the assembly's own parts use.
  static constexpr uint32_t kPartTable = 0xC0u;

  // camHoldTimer (+0x40, u16): the cooldown the performance leaves behind. Guest 0x80137198 stores 30
  //   here on every path that drops back to subState 0, and this leaf counts it down one per frame,
  //   testing the SIGN OF THE LOW 16 BITS — so the release fires on the frame the counter passes
  //   below zero, i.e. 31 frames after the performance ended, not 30.
  uint16_t camHoldTimer() const {
    return mCore->mem_r16(mAt + 0x40u);
  }
  void setCamHoldTimer(uint16_t v) const {
    mCore->mem_w16(mAt + 0x40u, v);
  }

  // seqMarker (+0x6A, s16): the performance's own sequence marker. FUN_80136F08 (state-0 init) zeroes
  //   it; FUN_80137198 sets it to 1 when it starts the routine (together with +0x6C) and steps it
  //   through 4/5 as the routine runs, testing bit0 of it as "running". For THIS leaf only its
  //   zero/non-zero sense matters, and it means exactly "the performance has run and its camera hold
  //   has not been released yet" — this leaf never sets it, it only clears it, together with the
  //   camera mode it is paired with.
  //   OFFSET 0x6A IS REUSED ACROSS UNRELATED NODE TYPES in this engine and means something different
  //   on each (a group id compared against 0x800BF817 in beh_visibility_gate_dispatch, a count, a
  //   packed nibble pair for SFX ids, a matrix column scale, an angle). Nothing from those transfers
  //   here; the meaning above comes only from this family's own sibling leaf.
  int32_t seqMarker() const {
    return mCore->mem_r16s(mAt + 0x6Au);
  }
  void setSeqMarker(uint16_t v) const {
    mCore->mem_w16(mAt + 0x6Au, v);
  }

  // FUN_80138A64 — the idle-sub-state tick. See the banner in assembly_companion.cpp.
  static void endCamHoldAndRearmOnStroke(Core *c);

  // FUN_801389C8 — the per-visible-frame rig pose. See the banner in assembly_companion.cpp.
  static void composeRigAndApplyPartScales(Core *c);

  static void registerOverrides();

private:
  Core *mCore;
  uint32_t mAt;
};
