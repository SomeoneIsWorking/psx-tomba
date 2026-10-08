// game/ai/assembly_companion.cpp — the natively-owned leaves of the ASSEMBLY COMPANION:
//   endCamHoldAndRearmOnStroke   (guest FUN_80138A64) — its idle-sub-state tick, banner immediately below
//   composeRigAndApplyPartScales (guest FUN_801389C8) — its per-visible-frame rig pose, banner further down
//
// ── AssemblyCompanion::endCamHoldAndRearmOnStroke, guest FUN_80138A64 ────────────────────────────
//
// WHAT IT IS, IN GAME TERMS
// A field assembly — the seaside water-pump/seesaw class, a swinging beam with an arm, a bucket and a
// counterweight — does not just animate itself. When it is built it also SPAWNS A SECOND, JOINTED
// ACTOR at its own position and keeps it as a child, and that companion performs a short routine each
// time the beam completes the first half of its stroke. The routine is meant to be watched: while it
// runs, the companion takes the field camera's vertical-look mode away from the ordinary follow
// height and pulls the look target 200 units DOWN, then hands it back afterwards.
//
// THIS function is the companion's IDLE tick — the one that runs on every frame it is NOT performing
// — and it is the whole arming-and-releasing half of that arrangement. It does exactly two things:
//
//   1. IT ENDS THE PREVIOUS PERFORMANCE'S CAMERA HOLD. When the performance (guest 0x80137198)
//      finishes it drops the companion back to this sub-state, leaves a 30-frame cooldown in the
//      node's +0x40 timer, and leaves its sequence marker at +0x6A still set — i.e. "the camera is
//      still mine". This leaf counts that timer down one per frame and, on the frame it passes below
//      zero, writes the camera mode back to 0 and clears the marker. So the lowered camera outlives
//      the routine by about half a second rather than snapping back with it.
//   2. IT RE-ARMS ON THE PARENT'S NEXT STROKE. Once nothing is held, it watches the parent assembly's
//      stroke-phase field: when the beam reports phase 1 (first half-stroke reached) with its flag
//      word clear, the companion claims the lowered camera mode again, zeroes its step counter and
//      advances its sub-state — which is what starts the next performance.
//
// THE CLAIM IS A COPY, NOT A CONSTANT — this is the detail that explains the whole design. The guest
// does not store a literal 1 into the camera byte; it stores the halfword it just loaded from the
// PARENT's stroke-phase field. Assembly phase and camera mode are one shared 0/1/2 vocabulary, and
// the companion is the wire between them. (The preceding branch has already proved that value is 1,
// so the store is byte-identical either way — but written as a copy it says what the engine means.)
//
// The two halves are sequenced, not independent: the release re-reads the marker so that a claim
// expiring THIS frame can be re-armed on the SAME frame, and the guard is one shared "is anything
// held" test. A role-0 companion is the quiet variant — it runs the same state machine but neither of
// its camera writes fires (see AssemblyCompanion::role), so the camera never moves for it.
//
// HOW IT WAS IDENTIFIED — FROM THE CALLERS AND THE SPAWN CHAIN, NOT FROM THE NEIGHBOURHOOD
// Being a 0x8013xxxx address in the A00 overlay says only which overlay it lives in. Every statement
// above comes from a link that can be re-walked:
//   * THE CALLER. Its single caller in A00 is overlay guest 0x80136D9C, already ported as
//     beh_pure_inner_dispatch (game/ai/beh_pure_inner_dispatch.cpp:65). It reaches this address from
//     exactly one place: outer state 1, inner sub-state node[5] == 0, node[3] != 3. That is what makes
//     this "the idle tick", and why role 3 is never seen here — role 3 goes to 0x8018CDC4 instead.
//   * THE SPAWN CHAIN, which is what names the node itself. The assembly FUN_8012EB54
//     (game/ai/beh_substate_edge_orchestrator.cpp; the class is identified as the seaside pumps in
//     game/ai/assembly_node.h) calls SubstateEdgeLeaves::spawnInnerDispatchChild — guest 0x8013892C,
//     game/ai/substate_edge_native.cpp, whose own banner already calls the result "THE ASSEMBLY'S
//     COMPANION NODE" — from its state-0 init leaf overlay guest 0x8012ED84 (authenticated executable/overlay evidence:
//     19119, inside the body that starts at :18777) and again from the OPN-overlay assembly hook
//     0x8018C820 (authenticated executable/overlay evidence). The spawner allocates through
//     Placement::spawnWithParent (guest FUN_80072DDC, game/world/placement.cpp), whose body contains
//     the decisive store `mem_w32(newNode + 16, a0)` — the new node's +0x10 IS the spawner. It then
//     writes 0x80136D9C into the new node's +0x1C handler slot and copies the assembly's position.
//     So node+0x10 read here is the parent assembly, as a fact.
//   * IT IS CONFIRMED ON LIVE STATE, not only statically. Scanning the committed 2 MB RAM dumps for
//     nodes whose +0x1C handler is 0x80136D9C finds exactly THREE, at 0x800FCCF8 / 0x800FCE00 /
//     0x800FCF08, identical across all 23 dumps. Each has node[2]==10 and node[0x28]==3 — matching
//     the spawner's own (a3=10, a1=3) arguments — roles 0/1/2, and a +0x10 pointing at
//     0x800FB858 / 0x800FB960 / 0x800FBA68, EVERY ONE of which has 0x8012EB54 as its own handler.
//     Their world x ≈ 5899 / 7003 / 7988 sit in the pump x-bands independently derived in
//     docs/kanban/cards/008-water-pump-seesaw-tomba-s-weight-doesn-t-pull-it.md.
//   * WHAT +0x76 AND +0x78 ARE ON THAT PARENT — established from the assembly's OWN writers, not from
//     this reader. 0x8018C820 sets +0x76 to 1 as it starts the stroke (beside +0x48=256, +0x4E=256)
//     and to 2 at the second phase (+0x48=-2560, +0x4E=512); 0x8012F5B4 promotes 1 -> 2 at the far
//     limit and clears it on every path that parks the beam; the init clears it right before spawning
//     this companion. +0x78 is a bitfield on the same pair (bit1, bit2 and bit15 are tested; bit2 is
//     cleared with an AND 0xFFFB), so `== 0` here reads as "the assembly has no stroke flags
//     pending". Written up on AssemblyNode::strokePhase/strokeFlags.
//   * WHAT 0x800BF821 IS. CutsceneCamera::pitch (guest FUN_8006D654) is its consumer and switches its
//     height target on it — see the constant's comment in assembly_companion.h. The sibling leaf
//     0x80137198 is the only other A00 writer, and its two writes corroborate the reading directly:
//     it releases the mode when the companion's visibility byte node[1] is 0 (culled ⇒ give the
//     camera back) and again on the path that returns to this sub-state.
//
// TRUE EXTENT: [0x80138A64, 0x80138B04), 0xA0 bytes / 40 instructions. Established three ways, and
// deliberately NOT by "the next gen function in the shard" — that test is false here, because the
// recorded binary evidence's shard split is not address order: the body sits in ov_a00_shard_1.c but the next
// function BY ADDRESS, 0x80138B04, is emitted in ov_a00_shard_0.c:20730, while the next function in
// shard_1 is the much later 0x80138C70. So:
//   1. authenticated instruction extents puts the body at authenticated executable/overlay evidence and
//      trims one folded-sibling tail line after the real `return;`.
//   2. Every label the body branches to lies in [0x80138A64, 0x80138AFC], the last being the return.
//   3. Disassembly of the overlay image confirms it instruction-for-instruction (A00.BIN, base
//      0x80108F9C): `jr ra` at 0x80138AFC with its delay-slot nop
//      at 0x80138B00, and 0x80138B04 opening a fresh prologue (`addiu sp, sp, -0x20` /
//      `sw s0, 0x10(sp)`) — so the extent cannot run on into it. 0x80138B04 is also its own entry in
//      the overlay dispatch table (authenticated executable/overlay evidence).
// frame_size = 0: the guest body never touches sp, so there is no guest stack frame to mirror
// (tools/binary ABI evidence 80138A64 --contract). abi_extract's 2 "unreachable blocks" are the
// recorded binary evidence's duplicated `return;` tail plus that folded-sibling line, not a folded-in sibling body.
//
// SHAPE OF THE PORT: a rebuild over the typed lens, in the panel_fill.cpp house style — argument
// register in, named fields out, no scratch-register maintenance. The static store sequence is
// unchanged from the guest (w16 +0x40, w8 camera, w16 +0x6A, w8 camera, w8 +0x06, w8 +0x05), which is
// what tools/dynamic differential evidence gates.
#include "assembly_companion.h"
#include "assembly_node.h"
#include "core.h"
#include "core/overrides/guest_jal.h"
#include "core/overrides/native_override_catalog.h"
#include "guest_abi.h"

namespace {
// The one stroke phase the companion arms on: the assembly's "first half-stroke reached" code.
constexpr int32_t kStrokePhaseFirstHalfReached = 1;

// --- what composeRigAndApplyPartScales below calls, by name ---------------------------------------
// The three leaves are all in MAIN, all already natively owned, and all reached through the shared
// dispatch choke point so the override registry decides native-vs-oracle for them too.
constexpr uint32_t kNodeXformBuildAxis = 0x80051C8Cu; // NodeXform::buildAxis   (game/render/node_xform.cpp)
constexpr uint32_t kNodeXformSeedDiag = 0x800517BCu;  // NodeXform::seedBlock   (game/render/node_xform.cpp)
constexpr uint32_t kMulMatrix0 = 0x80084250u;         // GteTransform3::rotate3AndPackIr
                                                      // (game/math/wide_re_gte_transform3.cpp) — libgte
                                                      // MulMatrix0(dst,src): dst = dst x src, three
                                                      // MVMVA passes, one per column of src.
// The jal-site return addresses, so a nested callee's own `sw ra` spills the guest's value.
constexpr uint32_t kRaAfterComposeRig = 0x801389ECu;
constexpr uint32_t kRaAfterSeedDiag = 0x80138A24u;
constexpr uint32_t kRaAfterMulMatrix = 0x80138A30u;

// The scratchpad slot the per-part diagonal scale matrix is built in. Same address
// game/render/node_xform.cpp:119 names kScrSrcMatrix and uses for exactly the same purpose (the
// diagonal-seeded source matrix of a scale compose); named again here because that one is file-local.
constexpr uint32_t kScrDiagScale = 0x1F800000u;

// Guest-stack frame for overlay guest 0x801389C8, table in program order — emitted by
//   python3 external/psxport/tools/binary ABI evidence 0x801389C8 --scaffold --guestabi
constexpr GuestFrameSpill kComposeRigSpills[6] = {
    {19, 44},
    {31 /*ra*/, 52},
    {20, 48},
    {18, 40},
    {17, 36},
    {16, 32},
};
} // namespace

// ORACLE: overlay guest 0x80138A64
void AssemblyCompanion::endCamHoldAndRearmOnStroke(Core *c) {
  const AssemblyCompanion companion(c, c->r[4]);

  // (1) Expire the hold the finished performance left behind. Role 0 never holds the camera, so it
  // never runs the countdown either — its marker, if set, simply keeps it parked.
  int32_t held = companion.seqMarker();
  if (held != 0 && companion.role() != 0) {
    const uint16_t remaining = (uint16_t)(companion.camHoldTimer() - 1u);
    companion.setCamHoldTimer(remaining);
    if ((int16_t)remaining < 0) {                // the guest tests the sign of the low 16 bits
      c->mem_w8(kCamPitchMode, kCamPitchNormal); // give the camera back
      companion.setSeqMarker(0);
    }
    held = companion.seqMarker(); // re-read: a claim expiring now may re-arm now
  }

  const AssemblyNode assembly(c, companion.parent());
  if (held != 0) {
    return; // still holding — stay idle
  }
  const int32_t phase = assembly.strokePhase();
  if (phase != kStrokePhaseFirstHalfReached) {
    return; // wait for the beam's first half-stroke
  }
  // (2) Re-arm. The claim is the assembly's own phase code copied into the camera mode; it is skipped
  // for role 0 and while the assembly raises any stroke flag, but the sub-state advances either way —
  // the performance starts, it just may start uncinematically.
  if (companion.role() != 0 && assembly.strokeFlags() == 0) {
    c->mem_w8(kCamPitchMode, (uint8_t)phase);
  }
  const uint8_t nextSubState = (uint8_t)(companion.subState() + 1u);
  companion.setStep(0);
  companion.setSubState(nextSubState);
}

// AssemblyCompanion::composeRigAndApplyPartScales, guest FUN_801389C8 — the per-visible-frame rig
// pose: compose the companion's jointed sub-parts, then fold each part's own scale into its matrix.
//
// WHAT IT IS, IN GAME TERMS
// The companion the pump assembly spawns is not one sprite: it is a JOINTED ACTOR built out of a
// handful of sub-parts (6, 9 or 10 of them depending on the companion's role — see role() in
// assembly_companion.h). Somebody has to turn "where the companion is standing and how each joint is
// currently angled" into the matrices the renderer draws those parts with, once per frame. THIS
// function is that step, and it is run on every frame the companion is on screen.
//
// It does two things, in this order:
//
//   1. IT POSES THE WHOLE RIG. One call to NodeXform::buildAxis (guest 0x80051C8C) composes the
//      companion's own world matrix from its local Euler triple — explicitly rotX then rotY then
//      rotZ — copies its local position straight into its world position, and then tail-calls
//      NodeXform::propagateAxis, which walks the companion's own sub-part table and chains each
//      part's frame matrix and frame position off either the companion (a root part) or an earlier
//      sibling part (a jointed one). That propagate step deliberately uses a UNIT scale: it seeds an
//      identity diagonal before every part's rotation.
//   2. IT THEN FOLDS EACH PART'S OWN SCALE IN. Unit scale is exactly what step 1 leaves behind, so
//      this function walks the same sub-part table a second time and, per part, builds the diagonal
//      matrix diag(scaleX, scaleY, scaleZ) in the scratchpad (NodeXform::seedBlock, 0x800517BC) and
//      post-multiplies the part's frame matrix by it in place (MulMatrix0, 0x80084250 — three MVMVA
//      passes, one per column of the diagonal). Scales are GTE 4.12 fixed point and every part is
//      born at 0x1000 = 1.0, so a rig that never rescales itself is unaffected; a part that squashes
//      or stretches (the pump companion's routine is a physical one) gets its size here, after its
//      pose, which is why the two steps are separate calls rather than one compose.
//
// The part count is RE-READ from the node on every iteration rather than cached — the loop bound is
// `lbu` from node+8 inside the loop body, not hoisted — so a callee that changed the rig size mid-walk
// would be honoured. Nothing in the current call graph does, but the port keeps the re-read.
//
// HOW IT WAS IDENTIFIED — FROM ITS CALLER AND ITS FIELD WRITERS
//   * THE ONE CALLER. Across all of authenticated executable/overlay evidence there is exactly one reference to this
//   address that is
//     not the overlay's own dispatch table: overlay guest 0x80136D9C (authenticated executable/overlay evidence),
//     the COMPANION's behaviour handler, already ported as game/ai/beh_pure_inner_dispatch.cpp:75. It
//     reaches here from one place — the tail of outer state 1, after whichever sub-state leaf ran,
//     guarded by `node[1] != 0`. node[1] is the companion's visibility byte (the sibling leaf
//     0x80137198 gives the camera back when it is 0, per the banner above). So: runs last, runs on
//     the companion, runs only while visible. That is a per-frame presentation step, not game logic.
//   * WHAT THE FIELDS IT READS ARE, from their WRITERS. node+8 = part count and node+0xC0 = the
//     part-pointer table are both written by the companion's state-0 init overlay guest 0x80136F08, which
//     reads the per-role count from the table at 0x8014A9A4, spawns that many parts through the
//     generic child spawner 0x8007AAE8, and stores each into node+0xC0+4*i. The SAME init seeds each
//     part's +0x38/+0x3A/+0x3C to 0x1000 — which is what identifies the three halfwords this
//     function loads as a SCALE triple in 4.12 fixed point, and node+0x18 as the part's matrix.
//     game/render/node_xform.cpp's `Node` lens, RE'd independently and long before this, names the
//     identical offsets childScaleX/Y/Z and frameMatrixPtr, and feeds childScale to the identical
//     diagonal seeder — two independent derivations agreeing.
//   * WHAT THE THREE CALLEES ARE. All three are already natively owned and named in this repo:
//     0x80051C8C = NodeXform::buildAxis, 0x800517BC = NodeXform::seedBlock (the diagonal seeder,
//     {x,0,y,0,z,0,0,0}), 0x80084250 = GteTransform3::rotate3AndPackIr, whose GTE op decodes as
//     MVMVA(mx=rotation, v=V0, cv=none, sf=1) run three times — i.e. libgte MulMatrix0, which is
//     also what game/camera/cutscene_camera.cpp:32 independently calls it.
//   * GHIDRA AGREES. the Ghidra evidence workflow on the resident-overlay dump decompiles it to exactly this
//     shape (scratch/decomp/companion_pose_801389C8.c): FUN_80051c8c(); then, guarded by
//     `param_1[8] != 0`, a do/while over `*(int*)(iVar3+0xc0)` calling FUN_800517bc(0x1f800000,
//     *(short*)(p+0x38), *(short*)(p+0x3a), *(short*)(p+0x3c)) and FUN_80084250(p+0x18, 0x1f800000).
//     Adjacency to 0x8013892C/0x80138A64 was NOT used as evidence for any of this.
//
// TRUE EXTENT: [0x801389C8, 0x80138A64), 0xA0 bytes / 40 instructions. Three ways, none of them
// "the next gen function in the shard" (which is false here — this body is in ov_a00_shard_0.c while
// the FOLLOWING function by address, 0x80138A64, is emitted in ov_a00_shard_1.c):
//   1. authenticated instruction extents: authenticated executable/overlay evidence, 38 live body lines.
//   2. Every branch target in the body (L_80138A08, L_80138A44) lies inside the range.
//   3. Disassembly of the overlay image itself (A00.BIN, base 0x80108F9C): `addiu sp,sp,-0x38` opens at 0x801389C8, `jr
//   ra` at 0x80138A5C with
//      `addiu sp,sp,0x38` in its delay slot at 0x80138A60, and 0x80138A64 starts a different function
//      (`lh v0,0x6a(a0)` — endCamHoldAndRearmOnStroke, above). The PRECEDING function ends at
//      0x801389C4, so the range cannot start earlier either.
// frame_size = 56 with six callee-saved spills, so the frame IS mirrored: GuestFrame descends sp,
// writes r19/ra/r20/r18/r17/r16 at +44/+52/+48/+40/+36/+32 in the guest's own program order, and
// restores on every exit. All five values the guest keeps live across a nested call (r16..r20) are
// held in GuestReg proxies rather than C++ locals, so the callees' own spills see the real values.
// ORACLE: overlay guest 0x801389C8
void AssemblyCompanion::composeRigAndApplyPartScales(Core *c) {
  GuestFrame<56, 6> frame(c, kComposeRigSpills);

  GuestReg<19> self(c); // the companion node — live across all three calls
  self = c->r[4];

  // (1) Pose the rig: the companion's own world matrix, then every sub-part chained off it at unit
  // scale. buildAxis tail-calls propagateAxis, so this one call does the whole skeleton.
  tomba::guest::dispatchJalToReturn(*c, kNodeXformBuildAxis, kRaAfterComposeRig);

  const AssemblyCompanion companion(c, self);
  GuestReg<17> partIndex(c);
  partIndex = 0; // guest sets this in the branch delay slot — on both paths
  if (companion.partCount() == 0) {
    return; // a rig with no parts: posing it was the whole job
  }

  // (2) Fold each part's own scale into the matrix step 1 just left at unit scale.
  GuestReg<20> diagScale(c);
  diagScale = kScrDiagScale;
  GuestReg<18> slot(c);
  slot = self; // cursor: slot + kPartTable addresses part [partIndex]
  GuestReg<16> partAddr(c);
  do {
    partAddr = c->mem_r32(slot + kPartTable);
    const AssemblyChild part(c, partAddr);

    c->r[4] = diagScale; // seedBlock(scratch, sx, sy, sz) -> diag(sx,sy,sz)
    c->r[5] = (uint32_t)part.scaleX();
    c->r[6] = (uint32_t)part.scaleY();
    c->r[7] = (uint32_t)part.scaleZ();
    slot += 4; // guest advances the cursor in the jal's delay slot
    tomba::guest::dispatchJalToReturn(*c, kNodeXformSeedDiag, kRaAfterSeedDiag);

    c->r[4] = part.frameMatrixPtr(); // MulMatrix0(frameMatrix, diag): scale it, in place
    c->r[5] = diagScale;
    tomba::guest::dispatchJalToReturn(*c, kMulMatrix0, kRaAfterMulMatrix);

    partIndex += 1;
  } while ((int32_t)(uint32_t)partIndex < (int32_t)companion.partCount());
}

void AssemblyCompanion::registerOverrides() {
  // Both are A00, from the image: of 23 authenticated MODE images only A00 holds a function at each
  // address (40 and 39 words), each inside A00's loaded text range.
  //
  // 0x80138A64 is a leaf -- `lh $v0,0x6a($a0)` and no $sp touch -- so it has no stack-allocating
  // prologue, and the tool's old entry rule read that as "no image owns this address" (issue 0015).
  // It sits exactly 8 bytes after a `jr $ra` in A00 and in no other image, the tool's second
  // recognised entry shape. Note the entry rule was widened, not the address moved: the preceding
  // function really does restore `$ra` and return at 0x80138A60 (`addiu sp,sp,0x38` in its delay
  // slot), and 0x80138A64 does open a different function, which is what this file's own TRUE EXTENT
  // note above already recorded.
  tomba::native::declareOverlayOverride("A00",
                                        0x80138A64u,
                                        "AssemblyCompanion::endCamHoldAndRearmOnStroke",
                                        &AssemblyCompanion::endCamHoldAndRearmOnStroke);
  tomba::native::declareOverlayOverride("A00",
                                        0x801389C8u,
                                        "AssemblyCompanion::composeRigAndApplyPartScales",
                                        &AssemblyCompanion::composeRigAndApplyPartScales);
}
