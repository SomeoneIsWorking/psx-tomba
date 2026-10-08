// game/world/graphics_bind.cpp — PC-native OBJECT RENDER-BIND subsystem. See graphics_bind.h.
#include "graphics_bind.h"
#include "cfg.h"
#include "core.h"
#include "core/entry/game_ctx.h"
#include "core/overrides/native_override_catalog.h" // tomba::native::declareOverride — the one native-override registry
#include "game.h"
#include "gte_math.h" // Math::rotmat — libgte RotMatrix (native, static)
#include "guest_call.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// (The native scene-data record this subsystem will fill per visible entity is the real
// SceneObject in game/render/scene_data.h, now in scope via game_ctx.h → render.h. The former
// local placeholder struct here was dead and is removed.)

// ====================================================================================================
// OBJECT RENDER-RECORD subsystem — per-object display-record allocation + init (shared by collectables
// and other entities; 15 call sites across the collectable handlers alone).
// ====================================================================================================
//
// FUN_8007AAE8, the render-record allocator, is RenderRecordPool::allocate (render_record_pool.h).

// FUN_80051B70 — per-object render-record INIT. Allocates a record (FUN_8007AAE8), zero/init its fields
// (scale 0x1000), stamps the object's render fields, and stores a data pointer computed from two .data
// tables indexed by (a1, a2). If the record pool is exhausted it sets obj[+4]=3 (despawn-pending) and
// returns 1. RE'd from disas 0x80051B70:
//   if ((s16)*0x800ED098 <= 0) { obj[+4] = 3; return 1; }
//   obj[+8]=1; obj[+9]=1; obj[+0xd]=0; obj[+0xb8]=obj[+0xba]=obj[+0xbc]=0x1000;
//   rec = alloc(); obj[+0xc0] = rec;
//   rec[+6]=-1; rec[+0]=rec[+2]=rec[+4]=rec[+8]=rec[+0xa]=rec[+0xc]=0; rec[+0x38]=rec[+0x3a]=rec[+0x3c]=0x1000;
//   base = *(0x800ECF58 + a1*4);  rec[+0x40] = base + *(base + a2*4 + 4);
//   return 0;
uint32_t GraphicsBind::recordInitBody(Core *c) {
  uint32_t obj = c->r[4], a1 = c->r[5], a2 = c->r[6];
  if (c->mem_r16s(0x800ED098u) <= 0) {
    c->mem_w8(obj + 4, 3);
    return 1;
  }
  c->mem_w8(obj + 8, 1);
  c->mem_w8(obj + 9, 1);
  c->mem_w8(obj + 0xd, 0);
  c->mem_w16(obj + 0xbc, 0x1000);
  c->mem_w16(obj + 0xba, 0x1000);
  c->mem_w16(obj + 0xb8, 0x1000);
  uint32_t rec = eng(c).graphicsBind.records.allocate(*c);
  c->mem_w32(obj + 0xc0, rec);
  c->mem_w16(rec + 6, 0xffff); // -1
  c->mem_w16(rec + 0, 0);
  c->mem_w16(rec + 2, 0);
  c->mem_w16(rec + 4, 0);
  c->mem_w16(rec + 8, 0);
  c->mem_w16(rec + 0xa, 0);
  c->mem_w16(rec + 0xc, 0);
  c->mem_w16(rec + 0x38, 0x1000);
  c->mem_w16(rec + 0x3a, 0x1000);
  c->mem_w16(rec + 0x3c, 0x1000);
  eng(c).graphicsBind.installSceneRecord(rec, a1, a2); // FUN_80051B04 inlined here in the guest instruction path
  return 0;
}
void GraphicsBind::recordAlloc() {
  Core *c = core;
  // Attack (a) attribution: log the C return-address (caller of recordAlloc()) when
  // PSXPORT_RECALLOC_TRACE=1. Combined with the [sbs] core-map line, tallies A-only ra's
  // to name the native caller responsible for the +3 pool delta at 0x800ED098.
  if (mTrace < 0) {
    mTrace = getenv("PSXPORT_RECALLOC_TRACE") ? 1 : 0;
  }
  if (mTrace) {
    void *ra = __builtin_return_address(0);
    cfg_logi("recalloc",
             "core=%p ra=%p cnt_before=%d stage=%08X",
             (void *)c,
             ra,
             (int)c->mem_r16s(0x800ED098u),
             c->mem_r32(0x801fe00c));
  }
  gctx(c)->verification.run(
      [](Core *core) {
        return eng(core).graphicsBind.records.allocate(*core);
      },
      0x8007AAE8u,
      "recallocverify",
      gctx(c)->verification.on("recallocverify"));
}
void GraphicsBind::recordInit() {
  Core *c = core;
  gctx(c)->verification.run(
      &GraphicsBind::recordInitBody, 0x80051B70u, "recinitverify", gctx(c)->verification.on("recinitverify"));
}

// FUN_80051B04 — two-level scene-data-table pointer resolve. Pure address arithmetic, no branches.
// RE'd verbatim from disas 0x80051B04..0x80051B30. Reads the sceneData table root at 0x800ECF58
// (same table Spawn::sceneEntity reads at offset +8 = table[2]), then indexes by classArg + itemArg
// and stashes (base + off) at rec[+0x40]. The guest instruction path's FUN_80051B70 (recordInit) inlines the same
// body at its tail — the extraction dedupes.
void GraphicsBind::installSceneRecord(uint32_t rec, uint32_t classArg, uint32_t itemArg) {
  Core *c = core;
  uint32_t base = c->mem_r32(0x800ECF58u + classArg * 4u);
  uint32_t off = c->mem_r32(base + itemArg * 4u + 4u);
  c->mem_w32(rec + 0x40, base + off);
}

// FUN_800517F8 — per-object RENDER-STATE UPDATE: build the object's transform, then snapshot its int16
// world position into the 32-bit render-position fields. RE'd from disas 0x800517F8 / cross-checked
// against authenticated executable/overlay evidence guest 0x800517F8 (ground truth for the frame shape):
//   addiu sp,-0x18; spill s0(obj)=sp+16, ra=sp+20            // FRAMED -- was missing here, see below
//   FUN_80085480(obj+0x54, obj+0x98);          ra=0x80051814u  // transform/matrix build (kept content)
//   obj[+0xac] = (s32)(s16)obj[+0x2e]; obj[+0xb0] = (s32)(s16)obj[+0x32]; obj[+0xb4] = (s32)(s16)obj[+0x36];
//   FUN_80051300(obj);                          ra=0x80051834u  // downstream render setup (kept content)
// The two callees stay PSX via typed runtime address dispatch; we own the control flow + the position snapshot.
//
// REGISTER FAITHFULNESS (2026-07-08 fix): this function's own frame was missing entirely -- the
// nested NodeXform::propagateRotmat() call (FUN_80051300, reached via typed runtime address dispatch(c,0x80051300u))
// spills whatever is CURRENTLY in c->r[16]/c->r[31] into ITS OWN frame at entry. Without this
// function descending its own real 24-byte frame and holding the CALLER's live r16/ra at +16/+20
// for the duration of the call, propagateRotmat's spill captures stale/wrong bytes -- a real,
// reproducible SBS residual. Mirrored per docs/faithful-execution.md (same pattern as
// game/render/node_xform.cpp's BuildFrame).
uint32_t GraphicsBind::renderUpdateBody(Core *c) {
  uint32_t obj = c->r[4];
  uint32_t s16 = c->r[16], sra = c->r[31];
  c->r[29] -= 24;
  c->mem_w32(c->r[29] + 16, s16);
  c->r[16] = obj;
  c->mem_w32(c->r[29] + 20, sra);

  c->r[31] = 0x80051814u;
  mathOf(c).rotmat(obj + 0x54, obj + 0x98);
  c->mem_w32(obj + 0xac, (uint32_t)c->mem_r16s(obj + 0x2e));
  c->mem_w32(obj + 0xb0, (uint32_t)c->mem_r16s(obj + 0x32));
  c->mem_w32(obj + 0xb4, (uint32_t)c->mem_r16s(obj + 0x36));
  c->r[4] = obj;
  c->r[31] = 0x80051834u;
  psx::cpu::dispatchGuestToReturn0(*c, 0x80051300u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  uint32_t ret = c->r[2];

  c->r[31] = c->mem_r32(c->r[29] + 20);
  c->r[16] = c->mem_r32(c->r[29] + 16);
  c->r[29] += 24;
  return ret;
}
void GraphicsBind::renderUpdate() {
  Core *c = core;
  gctx(c)->verification.run(
      &GraphicsBind::renderUpdateBody, 0x800517F8u, "rendupdverify", gctx(c)->verification.on("rendupdverify"));
}

// FUN_80077B38 — set an object's GEOMETRY-BLOCK pointer from a table. RE'd from disas 0x80077B38 (leaf):
//   ent = *(a1 + a2*4);  obj[+0x38] = ent;  obj[+0x0e] = (u16)ent[+2] & 0x3fff;  return that value.
uint32_t GraphicsBind::setGeomBody(Core *c) {
  uint32_t obj = c->r[4], tbl = c->r[5], idx = c->r[6];
  uint32_t ent = c->mem_r32(tbl + idx * 4u);
  uint32_t cnt = (uint32_t)(c->mem_r16(ent + 2) & 0x3fffu);
  c->mem_w32(obj + 0x38, ent);
  c->mem_w16(obj + 0x0e, (uint16_t)cnt);
  return cnt; // incidental v0 the guest instruction path leaves (callers treat this void)
}
void GraphicsBind::setGeom() {
  Core *c = core;
  gctx(c)->verification.run(
      &GraphicsBind::setGeomBody, 0x80077B38u, "setgeomverify", gctx(c)->verification.on("setgeomverify"));
}

// FUN_8006CBD0 — copy a 6-halfword TRANSFORM BLOCK from a1 into the scratchpad camera/transform block
// (0x1F8000D2/D6/DA) + the object's rotation fields (obj+0x3a/0x3e/0x42). RE'd from disas 0x8006CBD0 (leaf):
//   *0x1F8000D2 = a1[0]; *0x1F8000D6 = a1[1]; *0x1F8000DA = a1[2];
//   obj[+0x3a] = a1[3]; obj[+0x3e] = a1[4]; obj[+0x42] = a1[5];
uint32_t GraphicsBind::setXformBlkBody(Core *c) {
  uint32_t obj = c->r[4], src = c->r[5];
  c->mem_w16(0x1F8000D2u, c->mem_r16(src + 0));
  c->mem_w16(0x1F8000D6u, c->mem_r16(src + 2));
  c->mem_w16(0x1F8000DAu, c->mem_r16(src + 4));
  c->mem_w16(obj + 0x3a, c->mem_r16(src + 6));
  c->mem_w16(obj + 0x3e, c->mem_r16(src + 8));
  uint32_t last = c->mem_r16(src + 0xa);
  c->mem_w16(obj + 0x42, (uint16_t)last);
  return last; // incidental v0
}
void GraphicsBind::setXformBlk() {
  Core *c = core;
  gctx(c)->verification.run(
      &GraphicsBind::setXformBlkBody, 0x8006CBD0u, "setxblkverify", gctx(c)->verification.on("setxblkverify"));
}

// FUN_8004BD64 — per-object POSITION-COMPOSE + render-state refresh. RE'd from disas 0x8004BD64
// (args: a0=obj, a1=mode&0xff, a2=srcA, a3=srcB, [sp+0x10]=t0 offset vector):
//   mode 0: obj[+0x2e/32/36] = roundedAvg(srcA[+0x2c/30/34], srcB...)   // 32-bit reads, (s+sign)>>1
//   mode 1: obj[+0x2e/32/36] = srcA[+0x2c/30/34] + t0[+0/2/4]            // 16-bit reads, add
//   mode 2: obj[+0x2e/32/36] = srcB[+0x2c/30/34] + t0[+0/2/4]
//   (other mode: no position write)
//   if ((obj[+0x28] & 0x7f) != 0) FUN_800517F8(obj);                     // refresh render-state (owned)
// v0 the guest instruction path incidentally leaves = the last full (un-truncated) computed value (or 0x800517F8's
// return if the tail ran); we mirror it so the A/B v0 compare holds.
uint32_t GraphicsBind::posComposeBody(Core *c) {
  uint32_t obj = c->r[4], mode = c->r[5] & 0xffu, srcA = c->r[6], srcB = c->r[7];
  uint32_t t0 = c->mem_r32(c->r[29] + 0x10);
  uint32_t last = c->r[2]; // default (other mode): v0 unchanged
  if (mode == 0) {
    for (int i = 0; i < 3; i++) {
      uint32_t so = 0x2c + (uint32_t)i * 4, oo = 0x2e + (uint32_t)i * 4;
      int32_t s = (int32_t)c->mem_r32(srcA + so) + (int32_t)c->mem_r32(srcB + so);
      s = (s + (int32_t)((uint32_t)s >> 31)) >> 1;
      c->mem_w16(obj + oo, (uint16_t)s);
      last = (uint32_t)s;
    }
  } else if (mode == 1 || mode == 2) {
    uint32_t src = (mode == 1) ? srcA : srcB;
    for (int i = 0; i < 3; i++) {
      uint32_t so = 0x2c + (uint32_t)i * 4, oo = 0x2e + (uint32_t)i * 4;
      uint32_t sum = (uint32_t)c->mem_r16(src + so) + (uint32_t)c->mem_r16(t0 + (uint32_t)i * 2);
      c->mem_w16(obj + oo, (uint16_t)sum);
      last = sum;
    }
  }
  if ((c->mem_r8(obj + 0x28) & 0x7fu) != 0) {
    c->r[4] = obj;
    eng(c).graphicsBind.renderUpdate();
    return c->r[2];
  }
  return last;
}
void GraphicsBind::posCompose() {
  Core *c = core;
  gctx(c)->verification.run(
      &GraphicsBind::posComposeBody, 0x8004BD64u, "poscomposeverify", gctx(c)->verification.on("poscomposeverify"));
}

// ═════════════════════════════════════════════════════════════════════════════════════════════════
// UNWIRED DRAFT (2026-07-08 wide-RE wave, region 0x80050000-0x8005FFFF). Not registered anywhere
// (no tomba::native::declareOverride, no tomba::native::declareOverride), not SBS-gated — dead code until a frontier
// pass wires + verifies it.
// ═════════════════════════════════════════════════════════════════════════════════════════════════

// FUN_800519E0 — RE'd from authenticated executable/overlay evidence guest 0x800519E0 (48-byte frame: r16/r17/r18/r19/
// r20/r21/r22 + ra spilled at +16/+20/+24/+28/+32/+36/+40/+44). The ONLY call inside the body is to
// RenderRecordPool::allocate (FUN_8007AAE8, already native + frameless — confirmed via authenticated executable/overlay
// evidence: no r29 change, no branches), so this function's OWN guest-stack push/pop has no register- faithfulness
// consequence for a nested callee (unlike NodeXform::propagate/propagateRotmat) — but the push/pop of the CALLER's own
// live r16-r22/ra into guest RAM for the duration of this call is itself part of the byte-exact state (2026-07-08: was
// missing the frame entirely, added below).
//
// Ghidra's decompile (undefined4 FUN_800519e0(int obj,uint count,int *sceneBase,undefined2 *tmpl))
// matches the generated C exactly once the args are named:
//   if ((s16)*0x800ED098 < count) { obj[9]=0; obj[4]=3; return 1; }         // pool too small -> fail
//   obj[9] = obj[8] = (u8)count; obj[0xBC]=obj[0xBA]=obj[0xB8]=0x1000; obj[0xD]=0;
//   for (i = 0; i < count; i++) {
//     rec = recordAlloc();  obj[0xC0 + i*4] = rec;
//     rec[6] = tmpl[i*4+0]; rec[0] = tmpl[i*4+1]; rec[2] = tmpl[i*4+2]; rec[4] = tmpl[i*4+3];
//     rec[0x38] = rec[0x3A] = rec[0x3C] = 0x1000;                           // scale identity
//     rec[0x40] = sceneBase + *(u32*)(sceneBase + 4 + i*4);                  // sceneData resolve
//   }
//   return 0;
namespace {
// -48: +16 r16, +20 r17, +24 r18, +28 r19, +32 r20, +36 r21, +40 r22, +44 ra (recordArrayInit
// 0x800519E0, confirmed against authenticated executable/overlay evidence guest 0x800519E0).
struct RecordArrayInitFrame {
  Core *c;
  uint32_t s16, s17, s18, s19, s20, s21, s22, sra;
  explicit RecordArrayInitFrame(Core *c_)
      : c(c_), s16(c_->r[16]), s17(c_->r[17]), s18(c_->r[18]), s19(c_->r[19]), s20(c_->r[20]), s21(c_->r[21]),
        s22(c_->r[22]), sra(c_->r[31]) {
    c->r[29] -= 48;
    c->mem_w32(c->r[29] + 16, s16);
    c->mem_w32(c->r[29] + 20, s17);
    c->mem_w32(c->r[29] + 24, s18);
    c->mem_w32(c->r[29] + 28, s19);
    c->mem_w32(c->r[29] + 32, s20);
    c->mem_w32(c->r[29] + 36, s21);
    c->mem_w32(c->r[29] + 40, s22);
    c->mem_w32(c->r[29] + 44, sra);
  }
  ~RecordArrayInitFrame() {
    c->r[31] = c->mem_r32(c->r[29] + 44);
    c->r[22] = c->mem_r32(c->r[29] + 40);
    c->r[21] = c->mem_r32(c->r[29] + 36);
    c->r[20] = c->mem_r32(c->r[29] + 32);
    c->r[19] = c->mem_r32(c->r[29] + 28);
    c->r[18] = c->mem_r32(c->r[29] + 24);
    c->r[17] = c->mem_r32(c->r[29] + 20);
    c->r[16] = c->mem_r32(c->r[29] + 16);
    c->r[29] += 48;
  }
};
} // namespace

uint32_t GraphicsBind::recordArrayInit(uint32_t obj, uint32_t count, uint32_t sceneBase, uint32_t tmpl) {
  Core *c = core;
  RecordArrayInitFrame frame(c);
  if (c->mem_r16s(0x800ED098u) < (int32_t)count) {
    c->mem_w8(obj + 9, 0);
    c->mem_w8(obj + 4, 3);
    return 1;
  }
  c->mem_w8(obj + 9, (uint8_t)count);
  c->mem_w16(obj + 0xBC, 0x1000);
  c->mem_w16(obj + 0xBA, 0x1000);
  c->mem_w16(obj + 0xB8, 0x1000);
  c->mem_w8(obj + 8, (uint8_t)count);
  c->mem_w8(obj + 0xD, 0);
  uint32_t sceneCursor = sceneBase + 4u;
  for (uint32_t i = 0; i < (count & 0xffu); i++) {
    uint32_t rec = records.allocate(*c);
    c->mem_w32(obj + 0xC0 + i * 4u, rec);
    c->mem_w16(rec + 6, c->mem_r16(tmpl + i * 8u + 0));
    c->mem_w16(rec + 0, c->mem_r16(tmpl + i * 8u + 2));
    c->mem_w16(rec + 2, c->mem_r16(tmpl + i * 8u + 4));
    c->mem_w16(rec + 4, c->mem_r16(tmpl + i * 8u + 6));
    c->mem_w16(rec + 0x38, 0x1000);
    c->mem_w16(rec + 0x3A, 0x1000);
    c->mem_w16(rec + 0x3C, 0x1000);
    c->mem_w32(rec + 0x40, sceneBase + c->mem_r32(sceneCursor));
    sceneCursor += 4;
  }
  return 0;
}

// ─────────────────────────────────────────────────────────────────────────────────────────────────
// Wiring — recordArrayInit only (see graphics_bind.h for why this one leaf uses the standard
// tomba::native::declareOverride + tomba::native::declareOverride wiring instead of the c->game->verify.run() A/B gate
// the rest of this class uses). Direct same-shard callers confirmed via authenticated executable/overlay evidence,
// authenticated executable/overlay evidence, authenticated executable/overlay evidence; overlay typed runtime address
// dispatch callers confirmed via game/ai/beh_a06_scripted_actor.cpp, beh_sop_intro_lifted.cpp, beh_sop_intro_pilot.cpp,
// beh_sop_intro_narration.cpp, actor_zoned_attacker.cpp, beh_variant_actor_sm.cpp,
// beh_id_routed_dispatch.cpp, beh_flagbit_timer_machine.cpp.
namespace {
void eov_recordArrayInit(Core *c) {
  c->r[2] = eng(c).graphicsBind.recordArrayInit(c->r[4], c->r[5], c->r[6], c->r[7]);
}
void eov_recordAllocate(Core *c) {
  eng(c).graphicsBind.records.allocateForGuest(*c);
}
} // namespace

void GraphicsBind::registerOverrides(Game * /*game*/) {
  tomba::native::declareOverride(0x800519E0u, "GraphicsBind::recordArrayInit", eov_recordArrayInit);
  tomba::native::declareOverride(
      tomba2::world::RenderRecordPool::kAllocate, "RenderRecordPool::allocateForGuest", eov_recordAllocate);
}
