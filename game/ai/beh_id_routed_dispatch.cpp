// game/ai/beh_id_routed_dispatch.cpp — PC-native per-object BEHAVIOR handler FUN_80121978.
//
// Overlay handler (hottest still-PSX overlay handler ~x1592/field-frame on seaside; ~115 instr),
// prologue 0x80121978; `jr ra` at 0x80121B3C. Disassembled from scratch/ram/field_seaside.bin.
// Outer state machine on node[4] (a0):
//   STATE 0 : INIT — FUN_800519E0(node,18,*0x800ECFCC,0x8014C02C); on 0, FUN_80077C40(node,0x8014E4EC,0)
//             with node[0x3C]=*0x800ECFD0, then seed node[0x80..0x86]=140/280/128/256, node[0x44]=384,
//             node[11]=node[0x2B]=node[0x47]=0, and node[4] += 1.
//   STATE 1 : route node[3] to a per-id sub-behavior leaf (0->FUN_801225BC, 1->FUN_80122D58,
//             95->FUN_801220FC, 96->FUN_80121B44, 97->FUN_80121CF8, 98->FUN_80122CA4, 99->FUN_8018BF08;
//             all other ids none), then ALWAYS FUN_80122BF4(node) + node[0x2B]=0.
//   STATE 2 : nothing.   STATE 3 : FUN_8007A624(node).   STATE >=4 : nothing.
//
// Ownership model (identical to the siblings): CONTROL FLOW + the direct node WRITES owned native;
// every sub-behavior CALL stays reachable via typed runtime address dispatch (pure-PSX leaf). No leaf here takes a
// stack argument, so no frame mirroring is needed. The INIT data words live in resident RAM (read live, not embedded).
// Transcribed 1:1 as a register machine; the byte-exact A/B gate (full RAM+scratchpad vs original guest-body call) is
// the safety net. NO GTE/render.

#include "cfg.h"
#include "core.h"
#include "core/entry/game_ctx.h"
#include "core/overrides/guest_jal.h"
#include "core/overrides/native_override_catalog.h"
#include "guest_abi.h"
#include "guest_call.h"
#include "spawn.h" // class Spawn (eng(c).spawn.despawn / dispatch / spawnAndInit)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

constexpr uint32_t BEH_FN = 0x80121978u;

} // namespace
static constexpr GuestFrameSpill kSpills_80121978[3] = {
    {16, 16},
    {31 /*ra*/, 24},
    {17, 20},
}; // frame=32, abi_extract --scaffold --guestabi

// FUN_0x80122BF4 — keeps a point pinned 119 world units ABOVE a linked object, in that object's OWN
// rotated frame, re-derived every field frame. It is the common tail of this dispatcher's state-1
// routes, which is why it lives here beside them rather than in a file of its own.
//
// Gated on u8[node+0x0B] == 1, so it is inert unless the sub-behaviour that arms that byte has run.
//
// THE BUNDLED CALLER CHANGE WAS CHECKED AND DELIBERATELY NOT MADE. The RE plan wanted all EIGHT
// state-1 `typed guest call` call sites here upgraded to `guest_fn` with their jal-site ra constants, and
// the verifier expanded that recommendation. Both halves were then verified:
//
//  1. The ra constants are CORRECT. Read off ov_a00_shard_0.c:11380-11409 they are exactly
//     0x801225BC/0x80121AB8, 0x80122D58/0x80121AC8, 0x801220FC/0x80121AD8, 0x80121B44/0x80121AE8,
//     0x80121CF8/0x80121AF8, 0x80122CA4/0x80121B08, 0x8018BF08/0x80121B18 and the common tail
//     0x80122BF4/0x80121B20 — every one matching the plan.
//
//  2. It would change nothing that is measured. A stale ra only matters when something READS the
//     guest stack it gets spilled to, i.e. under the byte-exact compare. But this function is a
//     BehaviorDispatch TABLE entry (game/object/behavior_dispatch.cpp:129), and the verification
//     substrate leg bypasses native dispatch. Same reasoning, and same conclusion, as the earlier
//     "native orchestrator spills a stale ra" alarm recorded in docs/findings/sbs.md.
//
// So the upgrade is hygiene on a path the gate does not cover, not a correctness fix, and eight edits
// to a live dispatcher are not worth making blind. Recorded here WITH the verified constants so that
// whoever does want it has the derivation and does not repeat the analysis.
// ORACLE: overlay guest 0x80122BF4
void beh_id_routed_offset_point(Core *c) {
  c->r[29] = c->r[29] + (uint32_t)-40;
  c->mem_w32((c->r[29] + (uint32_t)28), c->r[19]);
  c->r[19] = c->r[4] + c->r[0];
  c->mem_w32((c->r[29] + (uint32_t)32), c->r[31]);
  c->mem_w32((c->r[29] + (uint32_t)24), c->r[18]);
  c->mem_w32((c->r[29] + (uint32_t)20), c->r[17]);
  c->mem_w32((c->r[29] + (uint32_t)16), c->r[16]);
  c->r[3] = (uint32_t)c->mem_r8((c->r[19] + (uint32_t)11));
  c->r[2] = c->r[0] + (uint32_t)1;
  {
    int _t = (c->r[3] != c->r[2]);
    c->r[5] = (uint32_t)8064u << 16;
    if (_t) {
      goto L_80122C88;
    }
  }
  c->r[18] = c->mem_r32((c->r[19] + (uint32_t)232));
  c->mem_w16((c->r[5] + (uint32_t)192), (uint16_t)c->r[0]);
  c->r[5] = c->r[5] + (uint32_t)192;
  c->r[2] = c->r[0] + (uint32_t)-119;
  c->r[17] = (uint32_t)8064u << 16;
  c->r[16] = c->r[17] + (uint32_t)200;
  c->r[6] = c->r[16] + c->r[0];
  c->mem_w16((c->r[5] + (uint32_t)2), (uint16_t)c->r[2]);
  c->mem_w16((c->r[5] + (uint32_t)4), (uint16_t)c->r[0]);
  c->r[31] = 0x80122C4Cu;
  c->r[4] = c->r[18] + (uint32_t)24;
  psx::cpu::dispatchGuestToReturn0(*c, 0x800844C0u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  c->r[2] = (uint32_t)c->mem_r16((c->r[17] + (uint32_t)200));
  c->r[3] = (uint32_t)c->mem_r16((c->r[18] + (uint32_t)44));
  c->r[2] = c->r[2] + c->r[3];
  c->mem_w16((c->r[19] + (uint32_t)78), (uint16_t)c->r[2]);
  c->r[2] = (uint32_t)c->mem_r16((c->r[16] + (uint32_t)2));
  c->r[3] = (uint32_t)c->mem_r16((c->r[18] + (uint32_t)48));
  c->r[2] = c->r[2] + c->r[3];
  c->mem_w16((c->r[19] + (uint32_t)80), (uint16_t)c->r[2]);
  c->r[2] = (uint32_t)c->mem_r16((c->r[16] + (uint32_t)4));
  c->r[3] = (uint32_t)c->mem_r16((c->r[18] + (uint32_t)52));
  c->r[2] = c->r[2] + c->r[3];
  c->mem_w16((c->r[19] + (uint32_t)82), (uint16_t)c->r[2]);
L_80122C88:;
  c->r[31] = c->mem_r32((c->r[29] + (uint32_t)32));
  c->r[19] = c->mem_r32((c->r[29] + (uint32_t)28));
  c->r[18] = c->mem_r32((c->r[29] + (uint32_t)24));
  c->r[17] = c->mem_r32((c->r[29] + (uint32_t)20));
  c->r[16] = c->mem_r32((c->r[29] + (uint32_t)16));
  c->r[29] = c->r[29] + (uint32_t)40;
  return;
  return;
}

void beh_id_routed_dispatch(Core *c) {
  GuestFrame<32, 3> frame(c, kSpills_80121978);
  uint32_t s0 = c->r[4];           // s0 = a0 (node)
  uint32_t st = c->mem_r8(s0 + 4); // node[4] = outer state

  if (st == 1) {
    goto S1;
  }
  if ((int32_t)st < 2) {
    if (st == 0) {
      goto S0;
    }
    goto Lret;
  }
  if (st == 2) {
    goto Lret;
  }
  if (st == 3) {
    goto S3;
  }
  goto Lret; // st >= 4

// ---------------- STATE 0 (INIT) ----------------
S0:
  tomba::guest::dispatchLeafToReturn(*c, 0x800519E0u, s0, 18, c->mem_r32(0x800ECFCCu), 0x8014C02Cu); // FUN_800519E0
  if (c->r[2] != 0) {
    goto Lret;
  }
  c->mem_w32(s0 + 0x3C, c->mem_r32(0x800ECFD0u)); // node[0x3C] = *0x800ECFD0 (delay-slot store)
  tomba::guest::dispatchLeafToReturn(*c, 0x80077C40u, s0, 0x8014E4ECu, 0); // FUN_80077C40(node, 0x8014E4EC, 0)
  c->mem_w16(s0 + 0x80, 140);
  c->mem_w16(s0 + 0x82, 280);
  c->mem_w16(s0 + 0x84, 128);
  c->mem_w16(s0 + 0x86, 256);
  c->mem_w8(s0 + 11, 0);
  c->mem_w8(s0 + 0x2B, 0);
  c->mem_w16(s0 + 0x44, 384);
  c->mem_w8(s0 + 0x47, 0);
  c->mem_w8(s0 + 4, (uint8_t)(c->mem_r8(s0 + 4) + 1)); // node[4] += 1
  goto Lret;

// ---------------- STATE 1 ----------------
S1:
  switch (c->mem_r8(s0 + 3)) { // node[3]
  case 0:
    tomba::guest::dispatchLeafToReturn(*c, 0x801225BCu, s0);
    break;
  case 1:
    tomba::guest::dispatchLeafToReturn(*c, 0x80122D58u, s0);
    break;
  case 95:
    tomba::guest::dispatchLeafToReturn(*c, 0x801220FCu, s0);
    break;
  case 96:
    tomba::guest::dispatchLeafToReturn(*c, 0x80121B44u, s0);
    break;
  case 97:
    tomba::guest::dispatchLeafToReturn(*c, 0x80121CF8u, s0);
    break;
  case 98:
    tomba::guest::dispatchLeafToReturn(*c, 0x80122CA4u, s0);
    break;
  case 99:
    tomba::guest::dispatchLeafToReturn(*c, 0x8018BF08u, s0);
    break;
  default:
    break; // 2..94, 100..255: no sub-behavior
  }
  tomba::guest::dispatchLeafToReturn(*c, 0x80122BF4u, s0); // common tail FUN_80122BF4(node)
  c->mem_w8(s0 + 0x2B, 0);                                 // node[0x2B] = 0 (delay-slot store)
  goto Lret;

// ---------------- STATE 3 ----------------
S3:
  eng(c).spawn.despawn(s0); // FUN_8007A624(node)
Lret:
  return;
}

// Wiring for the state-1 common tail above. A00, from the image: of 23 authenticated MODE images only
// A00 holds a function at 0x80122BF4 (44 words), and the address is inside A00's loaded text range.
void id_routed_leaves_install() {
  static bool done = false;
  if (done) {
    return;
  }
  done = true;
  tomba::native::declareOverlayOverride("A00", 0x80122BF4u, "beh_id_routed_offset_point", beh_id_routed_offset_point);
}
