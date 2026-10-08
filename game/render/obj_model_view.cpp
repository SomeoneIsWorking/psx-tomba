// game/render/obj_model_view.cpp — native port of FUN_800318A0, the MODEL-VIEW SETUP leaf that
// every effect-mesh draw runs immediately before it emits packets.
//
// WHAT IT DOES, in game terms. It takes one object's authored placement — where it is in the world,
// which way it is turned, and how big it is — and turns that into the single transform the GTE needs
// so that the mesh writer downstream can feed it raw MODEL-SPACE corner coordinates and get screen
// coordinates back. In order: build a rotation matrix from the object's three PSX angle units;
// squash/stretch that matrix per axis by three scale bytes; multiply it by the SCENE CAMERA's view
// rotation so the object is now expressed in camera space; rotate the object's world position by the
// same camera and add the camera's own translation; then load the result into GTE CR0-7 and leave a
// copy in the scratchpad compose area at 0x1F800000 that the downstream RTPT/RTPS calls read back.
// It is the "place this object in front of the camera" step, nothing more — it draws nothing.
//
// ─────────────────────────────────────────────────────────────────────────────────────────────────
// HOW IT WAS IDENTIFIED — from the CALL SITES, not the shape. A run of MVMVAs over a scratchpad
// matrix could be almost any compose step, so the identification is anchored on who calls it and
// what they do next:
//
//  1. THE CALLERS. There is no `jal 0x800318A0` anywhere in the guest MAIN.EXE text (grep over
//     authenticated executable/overlay evidence finds only the dispatch wrapper), which is why this address is reached
//     through typed runtime address dispatch and shows up as a guest instruction path-dependency hotspot rather than a
//     static call. Scanning the A00-resident RAM dumps for the encoded instruction (`jal` to 0x800318A0 == 0x0C00C628)
//     finds exactly FOUR call sites, all in the A00 overlay:
//         scratch/raw/bucket_f470.bin -> 0x8013CE6C, 0x8013D4F4, 0x8013ED28, 0x8013EF70
//     whose containing functions are FUN_8013CDD4, FUN_8013D454, FUN_8013ED08, FUN_8013EF58.
//     (scratch/raw/a4_now.bin holds a different overlay and finds four call sites of its own — same
//     count, same family, different addresses; the leaf is overlay-agnostic.)
//
//  2. WHAT EACH CALLER DOES NEXT. Ghidra headless on those four (scratch/decomp/xform_callers.c)
//     shows three of them calling this leaf and then IMMEDIATELY calling FUN_80027768 — the shared
//     effect-mesh record writer, which has no native producer (portmap
//     render-producer-effect-mesh). Three of the four are the A00-overlay mesh controllers
//     overlay guest 0x8013D454 / _8013ED08 / _8013EF58: each composes the node's transform into GTE
//     CR0-7 itself and then calls the shared writer FUN_80027768, and THIS is the leaf that does
//     that composing — i.e. the RE entry point for a real producer. The fourth caller, FUN_8013CDD4, is
//     already ported (game/render/widescreen_margin_quad.cpp) and calls this leaf through
//     typed guest call () before its own RTPT run — its comment at :154 guessed the behaviour correctly from
//     the scratchpad addresses alone; this port replaces the guess with the body.
//
//  3. WHAT THE ARGUMENTS ARE, read off those call sites rather than inferred:
//         FUN_8013ED08: composeIntoGte(node+0x2C, node+0x54, node+0x48)
//         FUN_8013EF58: composeIntoGte(node+0x2C, node+0x50, node+0x48)
//     node+0x2C/+0x30 is the packed world anchor (VX|VY, VZ) that the WHOLE effect family uses —
//     game/render/fx_sprite.cpp documents the same pair for the sprite emitters. FUN_8013D454 (the
//     water-jet controller, also described in fx_sprite.cpp) builds its scale byte on the stack from
//     a single uniform u16 and writes the SAME value into all three bytes, which is what identifies
//     a1 as a per-COLUMN (x/y/z) scale triple rather than anything else. a2 is an SVECTOR of angles:
//     FUN_8013CDD4 stages it as three bytes each multiplied by 10 — a PSX-angle-unit conversion.
//
//  4. THE TWO CALLEES, both already owned and both libgte primitives, which pins the first half:
//     0x80085480 = Math::rotmat (game/math/gte_math.cpp:221, RotMatrix) and 0x80084520 =
//     Math::matColScale (gte_math.cpp:500, the per-column fixed-point scale of a CR-packed 3x3).
//
// GROUND TRUTH FOR THE BODY: authenticated executable/overlay evidence (guest 0x800318A0) — the recorded binary
// evidence's per-instruction transcription, which is authoritative for COP2 traffic where Ghidra emits unresolvable
// setCopReg/copFunction pseudo-calls. Frame/spill/call contract from `tools/binary ABI evidence 800318A0 --contract`.
// Draft from `direct executable disassembly 800318A0`.
//
// TRUE EXTENT: 0x800318A0 .. 0x80031AC0 inclusive (137 instructions, `jr ra` at 0x80031ABC with
// `addiu sp,sp,0x30` in its delay slot — located by scanning the RAM dump forward from the entry).
// Straight-line: no branches, ONE exit, which is why abi_extract reports a single 48-byte frame
// close and its two "unreachable blocks" are just the recorded binary evidence's duplicated trailing `return`.
//
// Writes the guest's own GTE and scratchpad state; it reads no OT and every write is part of the
// byte-exact state.
//
// WHY THE TWO LEAVES ARE CALLED THROUGH guest 0x80085480 / guest 0x80084520 AND NOT mathOf(c).rotmat(...):
// identical to the reason spelled out in game/world/collision_resolve.cpp's header banner. Those
// substrate bodies descend guest stack frames the native methods do not mirror; going straight to
// the Math methods would leave the callees' frame bytes below our sp unwritten while substrate core
// B writes them, and SBS compares that memory. The guest call boundaries keep each leaf's own guest
// frame, and still reach the installed native through the override slot exactly as gen does.
//
// REGISTER RESIDUE, audited: gen leaves r2/r3/r4/r5/r7/r12-r14 dirty on return. Nothing consumes
// them — the function is void and all four call sites ignore v0 (they immediately set up fresh a0-a3
// for FUN_80027768 or for their own RTPT run) — so the scratch dataflow below is ordinary C++ locals
// rather than GuestReg proxies. The three registers that ARE live across the two calls (r16/r17/r18)
// stay in the register file, because a callee may spill its caller's callee-saved registers.
#include "obj_model_view.h"
#include "core.h"
#include "core/overrides/guest_jal.h"               // GuestFrame / GuestReg / guest_call
#include "core/overrides/native_override_catalog.h" // tomba::native::declareOverride
#include "game.h"
#include "guest_abi.h"
#include <cstdint>

namespace {

// ── the scratchpad blocks this leaf reads and writes ─────────────────────────────────────────────
// Names taken from game/render/perobj_dispatch.cpp, which already documents the same three blocks
// for the MAIN.EXE sibling of this compose (FUN_8003CDD8) — one vocabulary for one memory layout.
constexpr uint32_t kScratchpadBase = 0x1F800000u;
constexpr uint32_t kComposeBase = kScratchpadBase; // the composed model-view lives here
constexpr uint32_t kCamRot = 0x1F8000F8u;          // scene camera view rotation (CR-packed)
constexpr uint32_t kCamTrans = 0x1F80010Cu;        // scene camera view translation (3 s32)

// A PSX MATRIX is a CR-packed 3x3 of s16 (five words) followed by three s32 of translation:
//   word0 = M00 | M01<<16   word1 = M02 | M10<<16   word2 = M11 | M12<<16
//   word3 = M20 | M21<<16   word4 = M22
// so element M[row][col] sits at byte offset kRow<row> + col*kColStride. That is exactly the
// +0/+6/+12 triple with a +0/+2/+4 column bias that the MVMVA loop below walks.
constexpr uint32_t kRow0 = 0u;
constexpr uint32_t kRow1 = 6u;
constexpr uint32_t kRow2 = 12u;
constexpr uint32_t kColStride = 2u;
constexpr uint32_t kRotWords = 5u; // the CR-packed 3x3 occupies words 0..4

// ── GTE register numbers, so no raw index appears in the body ────────────────────────────────────
constexpr uint32_t kGteVxy0 = 0, kGteVz0 = 1;                   // data: the vector MVMVA multiplies
constexpr uint32_t kGteIr1 = 9, kGteIr2 = 10, kGteIr3 = 11;     // data: MVMVA's IR input/output
constexpr uint32_t kGteMac1 = 25, kGteMac2 = 26, kGteMac3 = 27; // data: MVMVA's 32-bit result
constexpr uint32_t kGteCrRot = 0;   // control 0..4 — the rotation matrix MVMVA's mx=ROT selects
constexpr uint32_t kGteCrTrans = 5; // control 5..7 — TRX/TRY/TRZ

// The two MVMVA encodings, both against the ROTATION control matrix with no translation vector and
// sf=1 (>>12). They differ only in which vector is multiplied — the same pair perobj_dispatch.cpp
// names for the MAIN.EXE sibling compose.
constexpr uint32_t kMvmvaRotCol = 0x4A49E012u;   // mx=ROT, v=IR  — one COLUMN of the local matrix
constexpr uint32_t kMvmvaWorldPos = 0x4A486012u; // mx=ROT, v=V0  — the object's world position

// ── this function's own guest stack ──────────────────────────────────────────────────────────────
// Frame contract from `binary ABI evidence 800318A0 --contract`, program order. Not hand-derived.
constexpr uint32_t kFrameBytes = 48;
constexpr GuestFrameSpill kSpills[4] = {{18, 40}, {17, 36}, {16, 32}, {31 /*ra*/, 44}};
// The three scale FACTORS are built as a guest-stack local and passed to Math::matColScale BY
// ADDRESS, so they must really be written to guest memory — a host-side array would leave these
// twelve bytes unwritten while substrate core B writes them.
constexpr uint32_t kLocalScaleCol0 = 16;
constexpr uint32_t kLocalScaleCol1 = 20;
constexpr uint32_t kLocalScaleCol2 = 24;

// The scale byte is a 1.12 fixed-point numerator carried in a single byte, so the guest recovers the
// real factor by shifting it up 2 (byte 0x400 would be unity — see the header for what the callers
// actually pass).
constexpr uint32_t kScaleByteShift = 2;

// jal-site return-address constants, from the same abi_extract contract.
constexpr uint32_t kRaRotMatrix = 0x800318D0u; // -> Math::rotmat       0x80085480
constexpr uint32_t kRaColScale = 0x80031904u;  // -> Math::matColScale  0x80084520

// The scratchpad compose area, reached through the register that identifies it. r16 holds the base
// for the whole body (callee-saved across both leaf calls), and the lens re-reads it on every access
// exactly as the guest does, so it can never go stale across a call — the same idiom as
// game/world/collision_resolve.cpp's Rec.
// ComposeArea now lives in obj_model_view.h — see that header for WHY (port_check
// harvests lens setters from game/**/*.h only).

} // namespace

// ORACLE: guest 0x800318A0 (tools/dynamic differential evidence equivalence-gate marker; see docs/port-framework.md)
void ObjModelView::composeIntoGte(Core *c) {
  GuestFrame<kFrameBytes, 4> frame(c, kSpills);

  // The three values that must survive the two leaf calls, held where the guest holds them.
  GuestReg<18> worldPos(c);
  worldPos = c->r[4]; // -> packed VX|VY at +0, VZ at +4
  GuestReg<17> scaleBytes(c);
  scaleBytes = c->r[5]; // -> three per-column scale bytes
  GuestReg<16> mtx(c);
  mtx = kComposeBase;
  const ComposeArea compose{c};

  // 1. LOCAL ROTATION. Math::rotmat(angles = a2, dst = the compose area) turns the object's three
  //    PSX angle units into a CR-packed 3x3.
  c->r[4] = c->r[6]; // a0 = the angle vector the caller passed as a2
  c->r[5] = mtx;     // a1 = destination
  tomba::guest::dispatchJalToReturn(*c, 0x80085480u, kRaRotMatrix);

  // 2. LOCAL SCALE, per column, in place. Each authored byte becomes a 1.12 factor.
  c->mem_w32(c->r[29] + kLocalScaleCol0, c->mem_r8(scaleBytes + 0) << kScaleByteShift);
  c->mem_w32(c->r[29] + kLocalScaleCol1, c->mem_r8(scaleBytes + 1) << kScaleByteShift);
  c->mem_w32(c->r[29] + kLocalScaleCol2, c->mem_r8(scaleBytes + 2) << kScaleByteShift);
  c->r[4] = mtx;                        // a0 = the matrix to scale
  c->r[5] = c->r[29] + kLocalScaleCol0; // a1 = the three factors, on our own guest stack
  tomba::guest::dispatchJalToReturn(*c, 0x80084520u, kRaColScale);

  // 3. COMPOSE WITH THE CAMERA. Load the scene camera's view rotation into the GTE's rotation
  //    control matrix, then push each COLUMN of the local matrix through MVMVA and store the result
  //    back over that same column: the compose area goes from LOCAL to CAMERA-RELATIVE in place.
  for (uint32_t i = 0; i < kRotWords; i++) {
    gte_write_ctrl(kGteCrRot + i, c->mem_r32(kCamRot + 4u * i));
  }

  // Unrolled deliberately, one block per column: the guest emits three straight-line copies and the
  // port_check equivalence gate compares the STATIC store sequence, so a loop here would collapse
  // nine guest halfword stores into three. (Column bias 0, then kColStride, then 2*kColStride.)
  gte_write_data(kGteIr1, compose.elem(kRow0 + 0 * kColStride));
  gte_write_data(kGteIr2, compose.elem(kRow1 + 0 * kColStride));
  gte_write_data(kGteIr3, compose.elem(kRow2 + 0 * kColStride));
  gte_op(c, kMvmvaRotCol);
  compose.setElem(kRow0 + 0 * kColStride, gte_read_data(kGteIr1));
  compose.setElem(kRow1 + 0 * kColStride, gte_read_data(kGteIr2));
  compose.setElem(kRow2 + 0 * kColStride, gte_read_data(kGteIr3));

  gte_write_data(kGteIr1, compose.elem(kRow0 + 1 * kColStride));
  gte_write_data(kGteIr2, compose.elem(kRow1 + 1 * kColStride));
  gte_write_data(kGteIr3, compose.elem(kRow2 + 1 * kColStride));
  gte_op(c, kMvmvaRotCol);
  compose.setElem(kRow0 + 1 * kColStride, gte_read_data(kGteIr1));
  compose.setElem(kRow1 + 1 * kColStride, gte_read_data(kGteIr2));
  compose.setElem(kRow2 + 1 * kColStride, gte_read_data(kGteIr3));

  gte_write_data(kGteIr1, compose.elem(kRow0 + 2 * kColStride));
  gte_write_data(kGteIr2, compose.elem(kRow1 + 2 * kColStride));
  gte_write_data(kGteIr3, compose.elem(kRow2 + 2 * kColStride));
  gte_op(c, kMvmvaRotCol);
  compose.setElem(kRow0 + 2 * kColStride, gte_read_data(kGteIr1));
  compose.setElem(kRow1 + 2 * kColStride, gte_read_data(kGteIr2));
  compose.setElem(kRow2 + 2 * kColStride, gte_read_data(kGteIr3));

  // 4. CAMERA-RELATIVE POSITION. Rotate the object's world anchor by the camera (the rotation
  //    control matrix is still the camera's — step 3 never wrote it back), park the raw MAC result
  //    in the compose area's translation slots, then add the camera's own translation on top. The
  //    guest really does store twice per axis; both stores are guest-visible state.
  gte_write_data(kGteVxy0, c->mem_r32(worldPos + 0));
  gte_write_data(kGteVz0, c->mem_r32(worldPos + 4));
  gte_op(c, kMvmvaWorldPos);
  compose.setTrans(0, gte_read_data(kGteMac1));
  compose.setTrans(1, gte_read_data(kGteMac2));
  compose.setTrans(2, gte_read_data(kGteMac3));
  compose.setTrans(0, compose.trans(0) + c->mem_r32(kCamTrans + 0));
  compose.setTrans(1, compose.trans(1) + c->mem_r32(kCamTrans + 4));
  compose.setTrans(2, compose.trans(2) + c->mem_r32(kCamTrans + 8));

  // 5. PUBLISH. The finished model-view goes into the GTE control registers — R into CR0-4, T into
  //    CR5-7 — which is the state every RTPT/RTPS in the draw that follows projects through.
  for (uint32_t i = 0; i < kRotWords; i++) {
    gte_write_ctrl(kGteCrRot + i, compose.rotWord(i));
  }
  gte_write_ctrl(kGteCrTrans + 0, compose.trans(0));
  gte_write_ctrl(kGteCrTrans + 1, compose.trans(1));
  gte_write_ctrl(kGteCrTrans + 2, compose.trans(2));
}

void ObjModelView::registerOverrides(Game *) {
  tomba::native::declareOverride(0x800318A0u, "ObjModelView::composeIntoGte", &ObjModelView::composeIntoGte);
}
