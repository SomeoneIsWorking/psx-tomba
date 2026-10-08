// perobj_dispatch.cpp — SUBSTRATE MIRROR for the per-object cmd-list dispatch chain
// FUN_8003CDD8 (Render::cmdListDispatch) -> FUN_8003F698 (Render::perModeDispatch).
//
// Ownership: address band 0x8003xxxx (this agent's exclusive band per the frontier task). Both
// addresses confirmed unowned via tools/codemap.py before porting.
//
// WHY these two and not the whole 0x8003CCA4 family: CCA4's 5 special-effect sub-cases
// (FUN_8003F4C4/F3F4/D584/F594/F344, none of which fire at seaside) stay substrate; CCA4's cases 0/4
// (the only ones seaside objects hit) call `FUN_8003cdd8` unconditionally as their entire body, so
// CDD8/F698 are a clean, non-conflicting slice of the SAME dispatch chain.
//
// CALL-SITE MECHANISM: unlike the walk-cluster addresses the override registry's typed runtime address dispatch
// interception targets (typed runtime address dispatch-only, nullptr setter), CDD8 and F698 are reached from CCA4's
// authenticated executable/overlay evidence as PLAIN INTRA-SHARD C CALLS (`guest 0x8003CDD8(c)` / `guest 0x8003F698(c)`
// — see authenticated executable/overlay evidence guest 0x8003CCA4 and authenticated executable/overlay evidence guest
// 0x8003CDD8). overrides::dispatch only intercepts inside typed runtime address dispatch, so it cannot see these calls;
// the ONLY interception point is the image-qualified runtime dispatcher slot each the cited guest address wrapper in
// shard_disp.c already checks (`if (image-qualified runtime dispatcher[N]) { image-qualified runtime dispatcher[N](c);
// return; }`). tomba::native::declareOverride() is that installer.
//
// RE (Ghidra headless decompile of a live free-roam RAM dump, scratch/bin/field_ram.bin, cross-checked
// against the guest body; the binary's GTE moves are ground truth, not Ghidra's setCopReg helpers.
//
// FUN_8003CDD8 (a0=node r4, a1=flag r5): for each active cmd on node's persistent render-command list
// (count @node+8, capacity @node+9, cmd ptr array @node+0xC0+4i):
//   - geomblk = cmd+0x40; skip (continue) if 0.
//   - stash the object's WORLD POSITION triple (cmd+0x2C/0x30/0x34, 3 s16) into scratch WORLD_POS
//     (0x1F8000C0/C2/C4).
//   - load the CAMERA rotation (scratch CAM_ROT 0x1F8000F8, 5-word CR-packed 3x3) into GTE CR0-4.
//   - for each of 3 columns of the object's LOCAL rotation (cmd+0x18, row-stride 6 col-stride 2):
//     write IR1-3, run MVMVA_ROTCOL (mx=ROT using CR0-4=camera, v=IR, cv=none, sf=1), store the
//     resulting IR1-3 into the composed matrix OBJ_ROT (scratch 0x1F800000, CR-packed).
//   - transform the stashed WORLD_POS by the camera (CR0-4 still camera; MVMVA_TRANS, v=V0, cv=none),
//     then ADD the camera translation offset (scratch CAM_TRANS 0x1F80010C/110/114) -> the composed
//     WORLD translation, stored at OBJ_ROT+20/24/28.
//   - reload CR0-4 from OBJ_ROT (the composed WORLD rotation) and CR5-7 from OBJ_ROT+20/24/28 (the
//     composed WORLD translation) — this is the transform the per-mode renderer submits vertices
//     through.
//   - OT base = the active OT, or its slot cmd[0x3F](s8) when node[0xD]&0xF == 4.
//   - dispatch (a0=geomblk, a1=otbase, a2=flag) to FUN_8003F698.
//
// FUN_8003F698 (a0=geomblk r4, a1=otbase r5, a2=flag r6): if the generic-force scratch flag
// (MODE_FORCE 0x1F800234) is clear AND (a2&1)==0, index the 22-entry mode table (MODE_TABLE
// 0x80015268) by the area's render-mode byte (MODE_BYTE 0x800BF870, bound-checked <22) and
// typed runtime address dispatch the resolved handler address (owned per-area leaves like 0x80146478 live outside this
// band — untouched, reached transparently via typed runtime address dispatch exactly as the guest instruction path
// reaches them). Otherwise (or mode>=22) fall back to guest 0x800803DC (the substrate's generic GT3/GT4 packet
// emitter).
#include "core.h"
#include "core/entry/game_ctx.h"
#include "core/overrides/guest_jal.h" // GuestFrame/guest_dispatch — perModeDispatch's demo migration (docs/port-framework.md)
#include "core/overrides/native_override_catalog.h"
#include "game.h"
#include "guest_abi.h"
#include "guest_call.h"
#include "guest_ordering_table.h"
#include "render.h"
namespace {
// Guest-stack frame RAII, mirroring guest 0x8003CDD8's real `addiu sp,-56` prologue (spills
// r16..r23/ra at +16..+48) and guest 0x8003F698's real `addiu sp,-24` prologue (spills ra only at
// +16) — see CLAUDE.md "MIRROR THE GUEST STACK". Neither function's own C++ body needs r16..r23 as
// meaningful cross-call state (register-faithfulness concern only applies to nested TAIL-CALLS that
// themselves spill a caller's live callee-saved regs — cmdListDispatch/perModeDispatch never set
// r16..r23 to a value a callee depends on), so this is the simple spill-live/restore-live idiom
// (same as game/render/cull.cpp's wrapFrame / perobj_billboard.cpp's GuestFrame), NOT the
// value-injecting variant node_xform.cpp needed.
struct CmdListFrame {
  Core *c;
  uint32_t s16, s17, s18, s19, s20, s21, s22, s23, sra;
  explicit CmdListFrame(Core *c_)
      : c(c_), s16(c_->r[16]), s17(c_->r[17]), s18(c_->r[18]), s19(c_->r[19]), s20(c_->r[20]), s21(c_->r[21]),
        s22(c_->r[22]), s23(c_->r[23]), sra(c_->r[31]) {
    c->r[29] -= 56;
    c->mem_w32(c->r[29] + 16, s16);
    c->mem_w32(c->r[29] + 20, s17);
    c->mem_w32(c->r[29] + 24, s18);
    c->mem_w32(c->r[29] + 28, s19);
    c->mem_w32(c->r[29] + 32, s20);
    c->mem_w32(c->r[29] + 36, s21);
    c->mem_w32(c->r[29] + 40, s22);
    c->mem_w32(c->r[29] + 44, s23);
    c->mem_w32(c->r[29] + 48, sra);
  }
  ~CmdListFrame() {
    c->r[31] = c->mem_r32(c->r[29] + 48);
    c->r[23] = c->mem_r32(c->r[29] + 44);
    c->r[22] = c->mem_r32(c->r[29] + 40);
    c->r[21] = c->mem_r32(c->r[29] + 36);
    c->r[20] = c->mem_r32(c->r[29] + 32);
    c->r[19] = c->mem_r32(c->r[29] + 28);
    c->r[18] = c->mem_r32(c->r[29] + 24);
    c->r[17] = c->mem_r32(c->r[29] + 20);
    c->r[16] = c->mem_r32(c->r[29] + 16);
    c->r[29] += 56;
  }
};
} // namespace

// DEMO MIGRATION (docs/port-framework.md validation #4): PerModeFrame's hand-rolled RAII replaced
// by runtime/psx/guest_abi.h's GuestFrame<FrameSize, NumSpills> — the contract-driven form
// tools/binary ABI evidence <addr> --scaffold --guestabi emits straight from guest 0x8003F698's real
// `addiu sp,-24` prologue (ra spill only, at sp+16). Behavior identical; this is purely the OPT-IN
// style swap the framework's deliverable 2 exists to validate (SBS-full 0-diff gate covers it).
static constexpr GuestFrameSpill kSpills_8003F698[1] = {{31 /*ra*/, 16}};

namespace {
constexpr uint32_t SCR = 0x1F800000u;        // PSX scratchpad base (the engine's GTE-compose temp area)
constexpr uint32_t WORLD_POS = SCR + 0xC0u;  // object world position stash (0x1F8000C0/C2/C4)
constexpr uint32_t CAM_ROT = 0x1F8000F8u;    // scene camera view rotation (CR-packed 3x3, 5 words)
constexpr uint32_t CAM_TRANS = 0x1F80010Cu;  // scene camera view translation (3 s32)
constexpr uint32_t OBJ_ROT = SCR;            // composed WORLD object rotation (CR-packed) + [+20/24/28] translation
constexpr uint32_t MODE_BYTE = 0x800BF870u;  // *this = render-mode select (0..0x15)
constexpr uint32_t MODE_FORCE = 0x1F800234u; // *this != 0 forces the generic GT3/GT4 path
constexpr uint32_t MODE_TABLE = 0x80015268u; // 22-entry jump table: mode -> per-mode renderer addr
// The generic GT3/GT4 packet emitter every non-routed cmd falls through to (still substrate).
constexpr uint32_t GENERIC_EMITTER = 0x800803DCu;
constexpr uint32_t MVMVA_ROTCOL = 0x4A49E012u; // MVMVA: camera-rot(CR0-4) x IR vector -> composed col
constexpr uint32_t MVMVA_TRANS = 0x4A486012u;  // MVMVA: camera-rot(CR0-4) x V0 (object world position)
} // namespace

// FUN_8003CDD8 — per-object cmd-list dispatch: composes the WORLD object transform (camera-rot x
// object-local, via MVMVA) into GTE CR0-7 for each active render command, then calls FUN_8003F698.
// ORACLE: guest 0x8003CDD8 (tools/dynamic differential evidence equivalence-gate marker; see docs/port-framework.md)
void Render::cmdListDispatch() {
  Core *c = mCore;
  CmdListFrame frame(c); // real -56 guest frame (RE: guest 0x8003CDD8 prologue) — descended even
                         // on the immediate-return path below, exactly like gen.
  const uint32_t node = c->r[4];
  const uint32_t flag = c->r[5];
  if (c->mem_r8(node + 8) == 0 || c->mem_r8(node + 9) == 0) {
    return;
  }
  // Exact control flow of the guest instruction path: count (node+8) is re-checked at the TOP of every
  // iteration (bails mid-list if the active count ever shrinks); capacity (node+9) gates only the
  // BACK-edge (post-increment), so it is never consulted before running i==0's body. Re-read both
  // fields fresh each time rather than caching (nothing in this body writes them, but this loop must
  // reproduce the guest instruction path's exact reads, not an equivalent-in-practice shortcut).
  int i = 0;
  for (;;) {
    if (i >= (int)c->mem_r8(node + 8)) {
      return;
    }
    const uint32_t cmd = c->mem_r32(node + 0xC0u + (uint32_t)i * 4);
    const uint32_t geomblk = c->mem_r32(cmd + 0x40u);
    if (geomblk != 0) {

      // Stash the object's world position (cmd+0x2C/0x30/0x34) for the translate MVMVA below.
      c->mem_w16(WORLD_POS + 0, c->mem_r16(cmd + 0x2Cu));
      c->mem_w16(WORLD_POS + 2, c->mem_r16(cmd + 0x30u));
      c->mem_w16(WORLD_POS + 4, c->mem_r16(cmd + 0x34u));

      // Load the CAMERA rotation into CR0-4.
      gte_write_ctrl(0, c->mem_r32(CAM_ROT + 0));
      gte_write_ctrl(1, c->mem_r32(CAM_ROT + 4));
      gte_write_ctrl(2, c->mem_r32(CAM_ROT + 8));
      gte_write_ctrl(3, c->mem_r32(CAM_ROT + 12));
      gte_write_ctrl(4, c->mem_r32(CAM_ROT + 16));

      // Compose the object's LOCAL rotation (cmd+0x18, 3 columns, row-stride 6 / col-stride 2) through
      // the camera rotation, one MVMVA per column, into OBJ_ROT (CR-packed 3x3).
      for (int col = 0; col < 3; col++) {
        const uint32_t base = cmd + 0x18u + (uint32_t)col * 2;
        gte_write_data(9, (uint32_t)c->mem_r16(base + 0));
        gte_write_data(10, (uint32_t)c->mem_r16(base + 6));
        gte_write_data(11, (uint32_t)c->mem_r16(base + 12));
        gte_op(c, MVMVA_ROTCOL);
        const uint32_t dst = OBJ_ROT + (uint32_t)col * 2;
        c->mem_w16(dst + 0, (uint16_t)gte_read_data(9));
        c->mem_w16(dst + 6, (uint16_t)gte_read_data(10));
        c->mem_w16(dst + 12, (uint16_t)gte_read_data(11));
      }

      // Transform the stashed world position by the (still-resident) camera rotation, add the camera
      // translation, and store the composed WORLD translation right after the rotation in OBJ_ROT.
      gte_write_data(0, c->mem_r32(WORLD_POS + 0)); // VXY0 = (posX, posY)
      gte_write_data(1, c->mem_r32(WORLD_POS + 4)); // VZ0  = posZ (low half)
      gte_op(c, MVMVA_TRANS);
      const int32_t tx = (int32_t)gte_read_data(25) + (int32_t)c->mem_r32(CAM_TRANS + 0);
      const int32_t ty = (int32_t)gte_read_data(26) + (int32_t)c->mem_r32(CAM_TRANS + 4);
      const int32_t tz = (int32_t)gte_read_data(27) + (int32_t)c->mem_r32(CAM_TRANS + 8);
      c->mem_w32(OBJ_ROT + 20, (uint32_t)tx);
      c->mem_w32(OBJ_ROT + 24, (uint32_t)ty);
      c->mem_w32(OBJ_ROT + 28, (uint32_t)tz);

      // Reload CR0-7 from the composed WORLD object transform for the per-mode renderer to consume.
      gte_write_ctrl(0, c->mem_r32(OBJ_ROT + 0));
      gte_write_ctrl(1, c->mem_r32(OBJ_ROT + 4));
      gte_write_ctrl(2, c->mem_r32(OBJ_ROT + 8));
      gte_write_ctrl(3, c->mem_r32(OBJ_ROT + 12));
      gte_write_ctrl(4, c->mem_r32(OBJ_ROT + 16));
      gte_write_ctrl(5, (uint32_t)tx);
      gte_write_ctrl(6, (uint32_t)ty);
      gte_write_ctrl(7, (uint32_t)tz);

      // OT base: sub-bucket by cmd+0x3F for node class 4.
      const auto ot = tomba2::render::OrderingTable::active(*c);
      uint32_t otbase = ot.base();
      if ((c->mem_r8(node + 0xDu) & 0xFu) == 4) {
        otbase = ot.slot((uint32_t)c->mem_r8s(cmd + 0x3Fu));
      }

      c->r[4] = geomblk;
      c->r[5] = otbase;
      c->r[6] = flag;
      // Register-faithfulness (f62 residual root cause, 2026-07-09): guest 0x8003CDD8 keeps r16..r23
      // LIVE as loop-invariant/loop-index scratch for its ENTIRE loop body (r16=i the loop counter,
      // r17=r23=SCR scratchpad base 0x1F800000, r18=node, r19=SCR+0xD0, r20=&OT base, r21=WORLD_POS,
      // r22=flag — verified against authenticated executable/overlay evidence guest 0x8003CDD8 lines 5119-5285). These
      // survive the nested guest 0x8003F698/guest 0x800803DC call chain via plain MIPS callee-save (never
      // explicitly reloaded before each per-iteration call). The still-substrate `guest 0x800803DC`
      // (unowned generic GT3/GT4 emitter) SPILLS the incoming r16/r17 to its own guest stack frame
      // (sp+16/sp+20) before reusing them as locals, then restores them on return — i.e. r16/r17's
      // CALLER value is genuine guest-stack-visible state, not dead scratch. Native cmdListDispatch used
      // C++ locals for `i`/`node`/`flag` and never wrote c->r[16..23], so guest 0x800803DC's prologue was
      // spilling STALE leftover register content instead of gen's real loop state — the exact SBS diff
      // at 0x801FE870..0x801FE878 (verified: gen's r16=i, r17=SCR match the two divergent words byte-
      // for-byte). Set the full live set here (not just r16/r17) since perModeDispatch's mode-table path
      // can reach OTHER still-substrate per-mode renderers that may equally depend on this callee-save
      // state.
      c->r[16] = (uint32_t)i;
      c->r[17] = SCR;
      c->r[18] = node;
      c->r[19] = SCR + 0xD0u;
      c->r[20] = tomba2::render::OrderingTable::kBasePointer;
      c->r[21] = WORLD_POS;
      c->r[22] = flag;
      c->r[23] = SCR;
      c->r[31] = 0x8003D07Cu; // RE'd return-address constant (guest 0x8003CDD8, right before guest 0x8003F698)
      // The command's current life is the drawn object; billboard callers reach this body without the dispatcher.
      const psx::present::EmissionScope::Guard command(
          c->emission, 0x8003CDD8u, eng(c).graphicsBind.records.object(cmd), 0);
      perModeDispatch();
    } // if (geomblk != 0)
    i++;
    if (!(i < (int)c->mem_r8(node + 9))) {
      return;
    }
  }
}

// MODE_TABLE's 22 entries are NOT the final FUN_ target addresses — they are addresses of F698's OWN
// internal case labels (jump-table entries the recorded binary evidence statically resolved when it built the
// switch below), confirmed by reading the live table out of a free-roam RAM dump
// (scratch/bin/field_ram.bin @0x80015268, all 22 words are one of these 11 literals). Each label's
// body immediately typed runtime address dispatch'es a fixed real target; 0x8003F788 is the "generic" label that falls
// through to guest 0x800803DC instead. This mapping is fixed game DATA (identical to the switch in
// authenticated executable/overlay evidence guest 0x8003F698), not something that varies at runtime.
static uint32_t perModeCaseTarget(uint32_t caseLabel) {
  switch (caseLabel) {
  case 0x8003F6E8u:
    return 0x80146478u;
  case 0x8003F6F8u:
    return 0x80132DC0u;
  case 0x8003F708u:
    return 0x8012555Cu;
  case 0x8003F718u:
    return 0x8013DAFCu;
  case 0x8003F728u:
    return 0x801362CCu;
  case 0x8003F738u:
    return 0x8013D568u;
  case 0x8003F748u:
    return 0x8012E1A0u;
  case 0x8003F758u:
    return 0x8012A9DCu;
  case 0x8003F768u:
    return 0x80116B14u;
  case 0x8003F778u:
    return 0x8010B1B8u;
  default:
    return 0; // 0x8003F788 (generic) or anything unrecognized -> fallback
  }
}

// RE'd return-address constant gen sets in r31 immediately before each case's dispatch call (see
// authenticated executable/overlay evidence guest 0x8003F698, labels L_8003F6E8.. — each is `caseLabel + 8`). Register-
// faithfulness (2026-07-09, the f118 residual root cause): a prior draft called typed runtime address dispatch without
// ever touching c->r[31], leaving whatever STALE value the caller (perObjRenderDispatch/cmdListDispatch)
// left there instead — a real, reproducible SBS diff at FUN_80146478's own ra spill slot
// (0x801FE8D0..). Mirrored below per CLAUDE.md ("MIRROR THE GUEST STACK... register-faithfulness").
static uint32_t perModeCaseReturnAddr(uint32_t caseLabel) {
  return caseLabel + 8u;
}

// WHICH GUEST EMITTER a cmd with this `flag` resolves to — the ONE encoding of FUN_8003F698's routing
// rule, consumed by perModeDispatch below.
//
// `*caseLabelOut` is the jump-table label the mode resolved to, or ZERO for the generic path — which
// is what tells perModeDispatch whether it dispatches through a label (needing that label's own RE'd
// return-address constant) or falls through to the generic emitter. The generic path deliberately
// reports caseLabel 0 for BOTH of its shapes: routing disabled/out-of-range, and the recognized
// generic label 0x8003F788 whose body is just `guest 0x800803DC(c)`.
uint32_t Render::resolvePerModeEmitter(Core *c, uint32_t flag, uint32_t *caseLabelOut) {
  *caseLabelOut = 0;
  if (c->mem_r8(MODE_FORCE) == 0 && (flag & 1u) == 0) {
    const uint32_t mode = c->mem_r8(MODE_BYTE);
    if (mode < 22) {
      const uint32_t caseLabel = c->mem_r32(MODE_TABLE + mode * 4);
      const uint32_t target = perModeCaseTarget(caseLabel);
      if (target != 0) {
        *caseLabelOut = caseLabel;
        return target;
      }
      // caseLabel == 0x8003F788 (or an unrecognized label) -> the guest instruction path's own `default:
      // typed runtime address dispatch(c, c->r[2])` would dispatch the RAW label address here; since 0x8003F788 IS the
      // generic-fallback label (whose body is just `guest 0x800803DC(c)`, no typed runtime address dispatch), reproduce
      // that directly rather than typed runtime address dispatch-ing a label address that has no guest entry.
      if (caseLabel != 0x8003F788u) {
        *caseLabelOut = caseLabel;
        return caseLabel;
      }
    }
  }
  return GENERIC_EMITTER;
}

// FUN_8003F698 — per-mode render dispatcher: routes to the area's per-mode renderer (mode-select byte
// + jump table) or the generic GT3/GT4 packet emitter (guest 0x800803DC).
void Render::perModeDispatch() {
  Core *c = mCore;
  GuestFrame<24, 1> frame(c, kSpills_8003F698); // real -24 guest frame (RE: guest 0x8003F698 prologue, ra spill only)
  const uint32_t flag = c->r[6];
  uint32_t caseLabel = 0;
  const uint32_t emitter = resolvePerModeEmitter(c, flag, &caseLabel);
  if (caseLabel != 0) {
    tomba::guest::dispatchJalToReturn(*c, emitter, perModeCaseReturnAddr(caseLabel));
    return;
  }
  c->r[31] = 0x8003F790u; // RE'd: L_8003F788's own r31 set before guest 0x800803DC (the generic label)
  psx::cpu::dispatchGuestToReturn0(*c, 0x800803DCu, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
}

namespace {
void ov_cmdListDispatch(Core *c) {
  rend(c)->cmdListDispatch();
}
void ov_perModeDispatch(Core *c) {
  rend(c)->perModeDispatch();
}
} // namespace

// ORACLE-PURITY FIX (2026-07-09, the f118 residual root cause): these two were installed via the RAW
// tomba::native::declareOverride — the per-Core image-qualified runtime dispatcher table shard_disp.c's guest
// 0x8003CDD8/guest 0x8003F698 wrappers consult on BOTH cores, with no oracle gate. That means SBS core B (supposed to
// be the pure original guest instructions substrate) was ALSO running this native code whenever guest 0x8003CCA4
// (correctly running pure on B via its own tomba::native::declareOverride registration) called guest 0x8003CDD8(c) —
// exactly the failure mode override_registry.h's own banner documents ("a trampoline that omitted its missing substrate
// guard silently ran native on the reference leg and turned SBS into a native-vs-native fake 0-diff"). Concretely:
// native Render::perObjRenderDispatch never mirrors guest 0x8003CCA4's `c->r[18] = node` prologue assignment (r18 is
// plain scratch to the native C++ body), so when B's PURE guest 0x8003CCA4 called into this SAME native
// cmdListDispatch, the CmdListFrame RAII spilled A's stale r18 instead of B's real node pointer — a genuine cross-core
// state leak, not just a byte diff. Fixed by routing through the shared override registry
// (tomba::native::declareOverride) like every other engine/game native in this call chain (perobj_billboard.cpp,
// overlay_gt3gt4.cpp, overlay_ground_gt3gt4.cpp, quad_rtpt_submit.cpp) — B now always runs the real guest 0x8003CDD8/
// guest 0x8003F698 bodies, closing the leak at its source.
void perobj_dispatch_install() {
  static bool done = false;
  if (done) {
    return;
  }
  done = true;
  tomba::native::declareOverride(
      0x8003CDD8u, "ov_cmdListDispatch", ov_cmdListDispatch, psx::present::Producer{psx::present::Arg::A0});
  tomba::native::declareOverride(0x8003F698u, "ov_perModeDispatch", ov_perModeDispatch);
}
