// class Collision — PC-native COLLISION-GRID subsystem.
// The collision-grid family that resolves an object's position against the level's spatial grid:
// the list-tail resolver (FUN_80031780), the grid row-pointer setup (FUN_80049968), the cell query /
// neighbor-walk (FUN_80047CBC), the resolve loop (FUN_800498C8), and the per-step origin/index setup
// (FUN_8004798C). Pure control flow over scratchpad + object/grid memory — NO GTE, NO render packets.
// Diagnostic A/B gates (listscan/gridsetup/gridquery/gridresolve/gridstep) are REPL channels,
// unchanged. The dispatched grid callees stay reachable by address (typed runtime address dispatch).
#include "collision.h"
#include "cfg.h"
#include "core.h"
#include "core/engine/engine.h"
#include "core/entry/game_ctx.h"
#include "core/overrides/native_override_catalog.h" // tomba::native::declareOverride — the one native-override registry
#include "execution_services.h"
#include "guest_call.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// GridRay names the shared collision scratchpad fields. Native owners and JIT-executed
// guest collision routines use this same memory contract.
namespace {
constexpr uint32_t GR = 0x1F800000u; // GridRay scratchpad base (gen spells it `8064u << 16`)
enum : uint32_t {
  GR_NORMAL_ANGLE = 416, // 0x1A0  flat-normal angle = ratan2(segment endpoints)
  GR_NORMAL_HI = 418,    // 0x1A2  (cleared by flatNormal)
  GR_CROSS = 420,        // 0x1A4  wall/line crossing coordinate (lineCross output)
  GR_CROSS_Z = 422,      // 0x1A6
  GR_CELL_ORG_X = 426,   // 0x1AA  cell origin X
  GR_CELL_ORG_Z = 428,   // 0x1AC  cell origin Z
  GR_SEG_X0 = 434,       // 0x1B2  segment endpoint 0 X
  GR_SEG_Z0 = 436,       // 0x1B4  segment endpoint 0 Z
  GR_SEG_X1 = 438,       // 0x1B6  segment endpoint 1 X
  GR_SEG_Z1 = 440,       // 0x1B8  segment endpoint 1 Z
  GR_PROBE_X = 444,      // 0x1BC  working probe X
  // 0x1BE was previously named "GR_EXTENT" (grid extent / probe-hi bound). That was wrong. It is the
  // probe's HEIGHT (Y). Two independent proofs: (a) snapObjectToTerrain and its sibling FUN_80048750
  // seed 0x1BC/0x1BE/0x1C0 from the probe object's ACT_WORLD_X/ACT_WORLD_Y/ACT_WORLD_Z triple, so the
  // middle field is Y by construction; (b) floorPick compares each candidate floor line's top edge
  // + 128 against this field to pick the line just under the probe — a height comparison. The grid's
  // real extents are the separate 0x1AE/0x1B0 pair that gridStep clamps against.
  GR_PROBE_Y = 446,    // 0x1BE  working probe Y (height)
  GR_PROBE_Z = 448,    // 0x1C0  working probe Z
  GR_LOCAL_X = 450,    // 0x1C2  slope-local X delta
  GR_SPAN = 452,       // 0x1C4  cross-span scratch (cross - extent)
  GR_LOCAL_Z = 454,    // 0x1C6  slope-local Z delta
  GR_LINE_TABLE = 472, // 0x1D8  line-record table base
  GR_LINE_ARRAY = 476, // 0x1DC  line-record array base
  GR_CELL_REC = 480,   // 0x1E0  current cell record (line-list idx@+2, count@+4)
  GR_BEST_LINE = 488,  // 0x1E8  chosen floor/wall line record (output)
  GR_LINE_CUR = 492,   // 0x1EC  working line-record cursor
};

// --- collision-CELL record layout (the 8-byte records the query walks; GR_CELL_REC points at one) ---
enum : uint32_t {
  CELL_TAG = 0, // halfword: the cell's tag/flag word
};
enum : uint16_t {
  CELL_SECTOR_LINK = 0x4000u, // this cell hands the object over to the sector named in the tag's low byte
};

// --- probe-OBJECT layout (the actor whose position is being resolved; a0 of the grid entry points) ---
// The world position is three 32-bit fixed-point coords at +0x2C/+0x30/+0x34; the grid works in whole
// units, so it reads and writes only their INTEGER halves at +0x2E/+0x32/+0x36 and leaves the
// fractions alone.
enum : uint32_t {
  ACT_SECTOR_ID = 42, // 0x2A (u8)  the path sector this actor is currently standing in
  ACT_WORLD_X = 46,   // 0x2E (u16) world X, integer half of the fixed coord at 0x2C
  ACT_WORLD_Y = 50,   // 0x32 (u16) world Y, integer half of the fixed coord at 0x30
  ACT_WORLD_Z = 54,   // 0x36 (u16) world Z, integer half of the fixed coord at 0x34
};

// GridRay lens — the same scratchpad addresses as the GR_* constants above, reached through named
// fields so a reader sees WHAT is being compared rather than which byte offset. Reads come in both
// signednesses because the guest bodies use both on the same field; keep whichever the guest used
// (a silent sign change here is a real behaviour change, not a cosmetic one).
struct GridRay {
  Core *c;

  int16_t normalAngle() const {
    return c->mem_r16s(GR + GR_NORMAL_ANGLE);
  }
  void setNormalAngle(uint16_t v) {
    c->mem_w16(GR + GR_NORMAL_ANGLE, v);
  }
  void setNormalHi(uint16_t v) {
    c->mem_w16(GR + GR_NORMAL_HI, v);
  }

  int16_t cross() const {
    return c->mem_r16s(GR + GR_CROSS);
  }
  void setCross(uint16_t v) {
    c->mem_w16(GR + GR_CROSS, v);
  }
  int16_t crossZ() const {
    return c->mem_r16s(GR + GR_CROSS_Z);
  }
  void setCrossZ(uint16_t v) {
    c->mem_w16(GR + GR_CROSS_Z, v);
  }

  uint16_t cellOrgX_u() const {
    return c->mem_r16(GR + GR_CELL_ORG_X);
  }
  int16_t cellOrgX() const {
    return c->mem_r16s(GR + GR_CELL_ORG_X);
  }
  uint16_t cellOrgZ_u() const {
    return c->mem_r16(GR + GR_CELL_ORG_Z);
  }
  int16_t cellOrgZ() const {
    return c->mem_r16s(GR + GR_CELL_ORG_Z);
  }

  uint16_t segX0_u() const {
    return c->mem_r16(GR + GR_SEG_X0);
  }
  int16_t segX0() const {
    return c->mem_r16s(GR + GR_SEG_X0);
  }
  int16_t segZ0() const {
    return c->mem_r16s(GR + GR_SEG_Z0);
  }
  int16_t segX1() const {
    return c->mem_r16s(GR + GR_SEG_X1);
  }
  int16_t segZ1() const {
    return c->mem_r16s(GR + GR_SEG_Z1);
  }

  uint16_t probeX_u() const {
    return c->mem_r16(GR + GR_PROBE_X);
  }
  int16_t probeX() const {
    return c->mem_r16s(GR + GR_PROBE_X);
  }
  void setProbeX(uint16_t v) {
    c->mem_w16(GR + GR_PROBE_X, v);
  }
  uint16_t probeZ_u() const {
    return c->mem_r16(GR + GR_PROBE_Z);
  }
  int16_t probeZ() const {
    return c->mem_r16s(GR + GR_PROBE_Z);
  }
  void setProbeZ(uint16_t v) {
    c->mem_w16(GR + GR_PROBE_Z, v);
  }

  int16_t probeY() const {
    return c->mem_r16s(GR + GR_PROBE_Y);
  }
  void setProbeY(uint16_t v) {
    c->mem_w16(GR + GR_PROBE_Y, v);
  }

  int16_t localX() const {
    return c->mem_r16s(GR + GR_LOCAL_X);
  }
  uint16_t localX_u() const {
    return c->mem_r16(GR + GR_LOCAL_X);
  }
  int16_t localZ() const {
    return c->mem_r16s(GR + GR_LOCAL_Z);
  }
  uint16_t localZ_u() const {
    return c->mem_r16(GR + GR_LOCAL_Z);
  }
  void setSpan(uint16_t v) {
    c->mem_w16(GR + GR_SPAN, v);
  }

  // The cell record the query latched (GR_CELL_REC): its first halfword is the cell TAG.
  uint16_t cellTag(uint32_t rec) const {
    return c->mem_r16(rec + CELL_TAG);
  }
  // A CELL_SECTOR_LINK cell hands the object over to another path sector; the tag's LOW BYTE is that
  // sector's id (little-endian, so the guest re-reads the same address as a byte to get it).
  uint8_t cellLinkSector(uint32_t rec) const {
    return (uint8_t)c->mem_r8(rec + CELL_TAG);
  }

  uint32_t lineTable() const {
    return c->mem_r32(GR + GR_LINE_TABLE);
  }
  uint32_t lineArray() const {
    return c->mem_r32(GR + GR_LINE_ARRAY);
  }
  uint32_t cellRec() const {
    return c->mem_r32(GR + GR_CELL_REC);
  }
  void setCellRec(uint32_t v) {
    c->mem_w32(GR + GR_CELL_REC, v);
  }
  uint32_t bestLine() const {
    return c->mem_r32(GR + GR_BEST_LINE);
  }
  void setBestLine(uint32_t v) {
    c->mem_w32(GR + GR_BEST_LINE, v);
  }
  uint32_t lineCur() const {
    return c->mem_r32(GR + GR_LINE_CUR);
  }
  void setLineCur(uint32_t v) {
    c->mem_w32(GR + GR_LINE_CUR, v);
  }
};

constexpr uint32_t ACT_NORMAL_COS = 72; // probe object + 0x48  <- rcos(angle) >> 4
constexpr uint32_t ACT_NORMAL_SIN = 76; // probe object + 0x4C  <- rsin(angle) >> 4

// ProbeActor lens — the probe object's grid-visible fields by name instead of by raw offset.
struct ProbeActor {
  Core *c;
  uint32_t obj;
  uint8_t sectorId() const {
    return (uint8_t)c->mem_r8(obj + ACT_SECTOR_ID);
  }
  uint16_t worldX() const {
    return c->mem_r16(obj + ACT_WORLD_X);
  }
  uint16_t worldY() const {
    return c->mem_r16(obj + ACT_WORLD_Y);
  }
  uint16_t worldZ() const {
    return c->mem_r16(obj + ACT_WORLD_Z);
  }
};

// Guest stack-frame slot offsets for the 32-byte frame FUN_8004766C opens (tools/binary ABI evidence
// --contract 8004766C). The port MIRRORS this frame — see snapObjectToTerrain.
enum : uint32_t {
  FR_S0 = 16, // sp+0x10  s0 (r16)
  FR_S1 = 20, // sp+0x14  s1 (r17)
  FR_S2 = 24, // sp+0x18  s2 (r18)
  FR_RA = 28, // sp+0x1C  ra (r31)
};
} // namespace

// FUN_80031780 — list-tail resolver / reset. Walks the 8-byte-stride linked list rooted at
// a0[52] (off 0x34), reading the tag word at entry+4 each step, until a tag has bit30|bit31
// (0xC0000000) set. If that terminator tag has bit30 (0x40000000) set -> clear the list
// (a0[52]=a0[56]=0); else set the tail pointer a0[56] (off 0x38)=found entry. If a0[52]==0 at
// entry it is a no-op. Pure guest-pointer/integer walk, no GP0/OT. `listscan` (lazy gate) A/B's
// the two written words.
void Collision::listScan(uint32_t obj) {
  Core *c = this->core;
  c->r[4] = obj; // taxi-in for the still-taxi verify super-call
  static int s_v = -1;
  if (s_v < 0) {
    s_v = cfg_dbg("listscan") ? 1 : 0;
  }
  uint32_t a0 = obj;
  uint32_t o52 = c->mem_r32(a0 + 52), o56 = c->mem_r32(a0 + 56);
  uint32_t n52 = o52, n56 = o56, v0 = c->r[2];
  if (o52 != 0) {
    uint32_t v1 = o52, a1;
    for (;;) {
      a1 = c->mem_r32(v1 + 4);
      bool brk = (a1 & 0xC0000000u) != 0;
      v1 += 8;
      if (brk) {
        break;
      }
    } // +8 is the loop's delay slot — runs even on exit
    v0 = a1 & 0x40000000u;
    if (v0) {
      n56 = 0;
      n52 = 0;
    } else {
      n56 = v1;
    }
  }
  if (s_v) {
    psx::cpu::callOriginalToReturn(*c,
                                   0x80031780u,
                                   psx::cpu::ExecutionBudget::currentTurn(*c),
                                   __func__); // memory untouched above -> oracle writes
    uint32_t r52 = c->mem_r32(a0 + 52), r56 = c->mem_r32(a0 + 56);
    static long ng = 0, nb = 0;
    if (r52 != n52 || r56 != n56) {
      if (nb++ < 20) {
        cfg_logi("listscan", "MISMATCH a0=%x 52 mine=%x oracle=%x  56 mine=%x oracle=%x", a0, n52, r52, n56, r56);
      }
    } else if (++ng % 5000 == 0) {
      cfg_logi("listscan", "%ld matches", ng);
    }
    return; // keep oracle result
  }
  c->mem_w32(a0 + 52, n52);
  c->mem_w32(a0 + 56, n56);
  c->r[2] = v0;
}

// FUN_80049968 — collision-grid ROW-POINTER setup. a0 = grid/layer index (&0xff). Reads the table
// base ptr @0x1F8001C8, indexes table[a0] (halfword offset) to a per-grid record, then writes 5
// scratchpad row pointers from the record's halfword fields:
//   0x1F8001CC = rec+0x14;  0x1F8001D0/D4/D8/DC = rec + rec[12/14/16/18]*2
// Pure pointer arithmetic over scratchpad + guest record data. `gridsetup` A/B's the 5 written words.
void Collision::gridSetup(uint32_t layer) {
  Core *c = this->core;
  c->r[4] = layer; // taxi-in for the verify super-call
  static int s_v = -1;
  if (s_v < 0) {
    s_v = cfg_dbg("gridsetup") ? 1 : 0;
  }
  uint32_t a0 = layer & 0xffu;
  uint32_t base = c->mem_r32(0x1F8001C8u);
  uint32_t rec = base + (uint32_t)c->mem_r16(base + a0 * 2) * 2;
  uint32_t cc = rec + 20;
  uint32_t d0 = rec + (uint32_t)c->mem_r16(rec + 12) * 2;
  uint32_t d4 = rec + (uint32_t)c->mem_r16(rec + 14) * 2;
  uint32_t d8 = rec + (uint32_t)c->mem_r16(rec + 16) * 2;
  uint32_t dc = rec + (uint32_t)c->mem_r16(rec + 18) * 2;
  if (s_v) {
    psx::cpu::callOriginalToReturn(*c, 0x80049968u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
    static long ng = 0, nb = 0;
    uint32_t o_cc = c->mem_r32(0x1F8001CCu), o_d0 = c->mem_r32(0x1F8001D0u), o_d4 = c->mem_r32(0x1F8001D4u),
             o_d8 = c->mem_r32(0x1F8001D8u), o_dc = c->mem_r32(0x1F8001DCu);
    if (o_cc != cc || o_d0 != d0 || o_d4 != d4 || o_d8 != d8 || o_dc != dc) {
      if (nb++ < 20) {
        cfg_logi("gridsetup",
                 "MISMATCH a0=%x cc=%x/%x d0=%x/%x d4=%x/%x d8=%x/%x dc=%x/%x",
                 a0,
                 cc,
                 o_cc,
                 d0,
                 o_d0,
                 d4,
                 o_d4,
                 d8,
                 o_d8,
                 dc,
                 o_dc);
      }
    } else if (++ng % 5000 == 0) {
      cfg_logi("gridsetup", "%ld matches", ng);
    }
    return;
  }
  c->mem_w32(0x1F8001CCu, cc);
  c->mem_w32(0x1F8001D0u, d0);
  c->mem_w32(0x1F8001D4u, d4);
  c->mem_w32(0x1F8001D8u, d8);
  c->mem_w32(0x1F8001DCu, dc);
}

// FUN_80047CBC — collision-grid CELL QUERY / neighbor-walk. Converts the probe position
// (sh[0x1BC],sh[0x1C0]) relative to grid origin (sh[0x1AA],sh[0x1AC]) into grid indices (>>6),
// bounds-checks against the row table (w[0x1CC]), looks up the cell record (w[0x1D0] + idx*8) and
// reads its tag. Then loops following the tag bits: 0x8000=keep walking, 0x4000=follow the cell's
// link/child list (inner sub-scan against u16[0x1BE]-32), else step ONE cell in +/-X (sh[0x1C0]) or
// +/-Z (sh[0x1BC]) per the low 3 tag bits, recompute the cell, repeat. Returns 0 (off-grid/blocked)
// or 1 (resolved). Writes scratchpad ONLY (0x08C idx, 0x1A8 tag, 0x1BC/0x1C0 stepped coords,
// 0x1E0/E4 cursor ptrs). t6=w[0x1D4], t7=u16[0x1BE], MASK=~63 (the -64 grid-snap mask).
static uint32_t grid_query_47cbc(Core *c) {
  GridRay gr{c};
  const uint32_t SP = 0x1F800000u, MASK = 0xFFFFFFC0u;
  // ---- phase A: initial cell from probe vs origin ----
  int32_t t1 = (gr.probeX() - gr.cellOrgX()) >> 6; // grid Z idx (a3/t1)
  int32_t a3 = t1;
  uint32_t row0 = c->mem_r32(SP + 0x1CC);
  uint32_t a1 = row0 + (uint32_t)(t1 << 2);        // &row0[t1] (4-byte stride)
  int32_t t0 = (gr.probeZ() - gr.cellOrgZ()) >> 6; // grid X idx (t0)
  uint32_t A1_0 = c->mem_r16(a1 + 0);
  if (t0 < (int32_t)A1_0) {
    return 0;
  }
  uint32_t a2 = (c->mem_r16(a1 + 2) + (uint32_t)t0) - A1_0;
  int32_t limit = (int32_t)((uint32_t)c->mem_r16(SP + 0x1AE) >> 6) - 2;
  if (a3 < limit) {
    if (!((a2 & 0xffff) < (uint32_t)c->mem_r16(a1 + 6))) {
      return 0;
    }
  }
  // ---- L_d64: latch the cell record + tag ----
  uint32_t idx = a2 & 0xffff;
  c->mem_w32(SP + 0x08C, idx);
  uint32_t ptr = c->mem_r32(SP + 0x1D0) + (idx << 3);
  a2 = c->mem_r16(ptr + 0);
  c->mem_w32(SP + 0x1E4, ptr);
  gr.setCellRec(ptr);
  c->mem_w16(SP + 0x1A8, (uint16_t)a2);
  if ((a2 & 0xc000u) != 0xc000u) {
    c->mem_w16(SP + 0x1A8, 0);
  }
  if ((a2 & 0x8000u) == 0) {
    return 1;
  }
  uint32_t t6 = c->mem_r32(SP + 0x1D4);
  uint32_t t7 = c->mem_r16(SP + 0x1BE);
  // ---- walk ----
  for (;;) {
    if (a2 & 0x4000u) {
      // ARM A: follow link / child list
      uint32_t rec = gr.cellRec(); // original record (a1)
      gr.setCellRec(t6 + ((uint32_t)c->mem_r16(rec + 2) << 3));
      if (a2 & 0x0001u) {
        int32_t a0 = 1;
        uint32_t cnt = c->mem_r16(rec + 4);
        if (1 < (int32_t)cnt) {
          int32_t a3p = (int32_t)t7 - 32;
          for (;;) {
            uint32_t cur = gr.cellRec() + 8;
            uint32_t iv = c->mem_r16(cur + 4);
            gr.setCellRec(cur);
            uint32_t iw = c->mem_r16(cur + 6);
            if (((iv - (uint32_t)a3p) & 0xffff) < iw) {
              break;
            }
            a0 += 1;
            if (!(a0 < (int32_t)cnt)) {
              break;
            }
          }
        }
        uint32_t t = c->mem_r16(rec + 6);
        bool hit = ((uint32_t)a0 == (t & 0xff)) || ((uint32_t)a0 == (t >> 8));
        if (!hit && (uint32_t)a0 == (uint32_t)c->mem_r16(rec + 4)) {
          hit = true;
        }
        if (hit) {
          gr.setCellRec(t6 + ((uint32_t)c->mem_r16(rec + 2) << 3));
        }
      }
      a2 = c->mem_r16(gr.cellRec() + 0);
    } else {
      // ARM B: step one grid cell, recompute
      if (a2 & 0x0004u) {
        switch (a2 & 3u) {
        case 1:
          gr.setProbeX((uint16_t)((gr.probeX_u() + 64) & MASK));
          t1++;
          break;
        case 0:
          gr.setProbeX((uint16_t)((gr.probeX_u() & MASK) - 1));
          t1--;
          break;
        case 2:
          gr.setProbeZ((uint16_t)((gr.probeZ_u() & MASK) - 1));
          t0--;
          break;
        case 3:
          gr.setProbeZ((uint16_t)((gr.probeZ_u() + 64) & MASK));
          t0++;
          break;
        }
      } else if ((uint32_t)c->mem_r16(SP + 0x1AE) < (uint32_t)c->mem_r16(SP + 0x1B0)) {
        if (a2 & 0x0002u) {
          gr.setProbeX((uint16_t)((gr.probeX_u() + 64) & MASK));
          t1++;
        } else {
          gr.setProbeX((uint16_t)((gr.probeX_u() & MASK) - 1));
          t1--;
        }
      } else {
        if (a2 & 0x0001u) {
          gr.setProbeZ((uint16_t)((gr.probeZ_u() + 64) & MASK));
          t0++;
        } else {
          gr.setProbeZ((uint16_t)((gr.probeZ_u() & MASK) - 1));
          t0--;
        }
      }
      // L_f9c: recompute cell from stepped indices
      uint32_t a1b = c->mem_r32(SP + 0x1CC) + (uint32_t)(((int32_t)(int16_t)t1) * 4);
      uint32_t A1b0 = c->mem_r16(a1b + 0);
      if (A1b0 == 0xffff) {
        return 0;
      }
      if ((int32_t)(int16_t)t0 < (int32_t)A1b0) {
        return 0;
      }
      uint32_t a2v = (c->mem_r16(a1b + 2) + (uint32_t)t0) - A1b0;
      uint32_t a0b = a2v & 0xffff;
      if (!(a0b < (uint32_t)c->mem_r16(a1b + 6))) {
        return 0;
      }
      uint32_t ptrB = c->mem_r32(SP + 0x1D0) + (a0b << 3);
      a2 = c->mem_r16(ptrB + 0);
      c->mem_w32(SP + 0x08C, a0b);
      c->mem_w32(SP + 0x1E4, ptrB);
      gr.setCellRec(ptrB);
      c->mem_w16(SP + 0x1A8, (uint16_t)a2);
    }
    if ((a2 & 0x8000u) == 0) {
      return 1;
    }
  }
}

int Collision::gridQuery() {
  Core *c = this->core;
  static int s_v = -1;
  if (s_v < 0) {
    s_v = cfg_dbg("gridquery") ? 1 : 0;
  }
  if (!s_v) {
    int r = (int)grid_query_47cbc(c);
    c->r[2] = (uint32_t)r;
    return r;
  }
  const uint32_t LO = 0x1F800080u, HI = 0x1F8001F0u, N = HI - LO;
  uint8_t snap[0x170], after[0x170];
  for (uint32_t a = LO; a < HI; a++) {
    snap[a - LO] = c->mem_r8(a);
  }
  uint32_t mine = grid_query_47cbc(c);
  for (uint32_t a = LO; a < HI; a++) {
    after[a - LO] = c->mem_r8(a);
    c->mem_w8(a, snap[a - LO]);
  } // capture+restore
  psx::cpu::callOriginalToReturn(*c, 0x80047CBCu, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  uint32_t oracle = c->r[2];
  int firstoff = -1;
  for (uint32_t a = LO; a < HI; a++) {
    if (c->mem_r8(a) != after[a - LO]) {
      firstoff = (int)(a - LO);
      break;
    }
  }
  static long ng = 0, nb = 0;
  if (firstoff >= 0 || mine != oracle) {
    if (nb++ < 30) {
      cfg_logi("gridquery", "MISMATCH ret mine=%x oracle=%x scratchdiff@+%x", mine, oracle, firstoff);
    }
  } else if (++ng % 2000 == 0) {
    cfg_logi("gridquery", "%ld matches", ng);
  }
  (void)N;
  c->r[2] = oracle; // keep oracle scratchpad state
  return (int)oracle;
}

// FUN_800498C8 — collision-grid RESOLVE LOOP (top of the grid family; pairs with the owned
// FUN_80049968 setup + FUN_80047CBC query). a0 = probe object. Iterates:
//   jal 0x8004798C(obj)                    -- per-step grid-origin/index setup (kept dispatched; non-trivial)
//   jal 0x80049968(u8 @0x1F8001FE)         -- row-pointer setup (owned Collision::gridSetup)
//   v0 = jal 0x80047CBC()                  -- cell query/neighbor-walk (owned Collision::gridQuery)
//   if v0 == 0 -> return 0                  (query found nothing / off-grid -> done)
//   v1 = w[0x1F8001E0] (the cell record ptr the query latched)
//   if (h[v1] & 0x4000) == 0 -> return 1   (resolved cell is terminal -> done, keep)
//   obj[42] = b[v1]                         (record the resolved cell's tag byte onto the probe object)
//   reload v1' = w[0x1F8001E0]; if (h[v1'] & 0x4000) != 0 -> LOOP (descend further)
//   else -> return 1
// Pure control flow over scratchpad + object memory; ONE object write (obj+42); NO GTE, NO render
// packets. The three callees stay PSX via typed runtime address dispatch (the two grid leaves honor their own owned
// override identically in the dispatched path). Return: 0 only when the query returns 0; otherwise 1.
static uint32_t grid_resolve_498c8(Core *c, uint32_t obj) {
  Collision &col = eng(c).collision;
  for (;;) {
    col.gridStep(obj);                               // per-step grid-origin/index setup — native
    col.gridSetup((uint32_t)c->mem_r8(0x1F8001FEu)); // row-ptr setup — native
    col.gridQuery();                                 // cell query — native
    if (c->r[2] == 0) {
      return 0;
    }
    uint32_t v1 = c->mem_r32(0x1F8001E0u);
    if ((c->mem_r16(v1) & 0x4000u) == 0) {
      return 1;
    }
    c->mem_w8(obj + 42, c->mem_r8(v1)); // record tag byte onto the object
    uint32_t v1b = c->mem_r32(0x1F8001E0u);
    if ((c->mem_r16(v1b) & 0x4000u) != 0) {
      continue; // bne v0,zero,0x800498e8 -> loop
    }
    return 1;
  }
}

int Collision::gridResolve(uint32_t obj) {
  Core *c = this->core;
  c->r[4] = obj; // taxi-in for the verify super-call
  static int s_v = -1;
  if (s_v < 0) {
    s_v = cfg_dbg("gridresolve") ? 1 : 0;
  }
  if (!s_v) {
    int r = (int)grid_resolve_498c8(c, obj);
    c->r[2] = (uint32_t)r;
    return r;
  }
  // Full RAM+scratchpad A/B vs original guest-body call. The native path runs first, its writes are snapshotted
  // and rolled back, then the guest instruction path runs and we diff. The dispatched callees (incl. the deep
  // FUN_8004798C tree) run in BOTH passes; FUN_800498C8's own 32-byte stack frame [sp-32, sp) is dead
  // below sp on return (gen saves regs there; native never touches the guest stack) -> excluded.
  static uint8_t *ram0 = (uint8_t *)malloc(0x200000);
  static uint8_t *ramN = (uint8_t *)malloc(0x200000);
  uint8_t spad0[0x400], spadN[0x400];
  uint32_t regs0[32];
  memcpy(regs0, c->r, sizeof regs0);
  memcpy(ram0, c->ram, 0x200000);
  memcpy(spad0, c->scratch, 0x400);
  uint32_t v0_n = grid_resolve_498c8(c, obj);
  memcpy(ramN, c->ram, 0x200000);
  memcpy(spadN, c->scratch, 0x400);
  memcpy(c->ram, ram0, 0x200000);
  memcpy(c->scratch, spad0, 0x400);
  memcpy(c->r, regs0, sizeof regs0);
  psx::cpu::callOriginalToReturn(*c, 0x800498C8u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  uint32_t v0_o = c->r[2];
  uint32_t sp = regs0[29] & 0x1FFFFFu, flo = (sp >= 0x800) ? sp - 0x800 : 0;
  int ro = -1;
  for (uint32_t a = 0; a < 0x200000; a++) {
    if (c->ram[a] != ramN[a] && !(a >= flo && a < sp)) {
      ro = (int)a;
      break;
    }
  }
  int so = -1;
  for (uint32_t a = 0; a < 0x400; a++) {
    if (c->scratch[a] != spadN[a]) {
      so = (int)a;
      break;
    }
  }
  static long ng = 0, nb = 0;
  if (ro >= 0 || so >= 0 || v0_n != v0_o) {
    if (nb++ < 40) {
      cfg_logi("gridresolve", "MISMATCH obj=%08x v0 n=%x o=%x ram@%x spad@%x sp=%x", obj, v0_n, v0_o, ro, so, sp);
    }
  } else if (++ng % 2000 == 0) {
    cfg_logi("gridresolve", "%ld matches", ng);
  }
  return (int)v0_o;
}

// FUN_8004798C — collision-grid PER-STEP ORIGIN/INDEX SETUP (the remaining dispatched callee inside
// the owned FUN_800498C8 resolve loop; completes the grid family with FUN_80049968 setup / FUN_80047CBC
// query / FUN_800498C8 resolve). a0 = probe object. Pure scratchpad halfword arithmetic + two dispatched
// callees; NO GTE, NO render packets. Scratchpad fields (base 0x1F800000):
//   0x1AA,0x1AC   = grid origin (X,Z)
//   0x1AE,0x1B0   = grid extents (X,Z)    [used unsigned in the select/clamp tests]
//   0x1B2,0x1B4   = grid cell base (X,Z)
//   0x1BA         = grid cell pitch       [signed; the >>14 fixed-point recompute multiplier]
//   0x1BC,0x1C0   = working probe coords (X,Z)
//   0x1FE (byte)  = current grid id
// Control flow:
//   if (obj[42] != byte[0x1FE]) jal 0x80048ecc(a0 = obj[42])    -- reload grid for this id (dispatched)
//   SELECT/RANGE TEST: if (h[0x1AE] u< h[0x1B0]) use the Z range else the X range; if probe is past the
//     selected range, jal 0x80048fc4(a0 = obj, a1 = 1)          -- re-resolve (dispatched)
//   CLAMP + RECOMPUTE: on (h[0x1AE] u< h[0x1B0]) -> Z branch (clamp 0x1C0 into [0x1AC, 0x1AC+0x1B0],
//     recompute 0x1BC) else X branch (clamp 0x1BC into [0x1AA, 0x1AA+0x1AE], recompute 0x1C0).
//   recompute writes the OTHER coord = cellbase + (((clamped - cellbase2) * pitch) >> 14) (signed mult,
//   low word). NB the >>14 is an arithmetic shift of the 32-bit low product (sra).
// `gridstep` gate = full RAM+scratchpad A/B vs original guest-body call (the two dispatched callees run in BOTH
// passes; this fn's own [sp-24, sp) stack frame + the callees' frames below sp differ harmlessly, so the
// gate excludes [sp-0x800, sp) — same family rationale as gridresolve/scriptvm).
static void grid_step_4798c(Core *c, uint32_t obj) {
  GridRay gr{c};
  const uint32_t SP = 0x1F800000u;
  // ---- block 1: reload grid if the object's recorded id differs ----
  uint32_t v1 = c->mem_r8(obj + 42);
  uint32_t gid = c->mem_r8(SP + 0x1FE);
  if (v1 != gid) {
    c->r[4] = v1;
    psx::cpu::dispatchGuestToReturn0(*c, 0x80048eccu, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  }
  // ---- block 2: select range (Z if h[0x1AE] u< h[0x1B0], else X), test, maybe re-resolve ----
  uint32_t aE = c->mem_r16(SP + 0x1AE); // h[0x1AE] (a1)
  uint32_t b0 = c->mem_r16(SP + 0x1B0); // h[0x1B0] (a0)
  uint32_t test;
  if (aE < b0) { // sltu(a1,a0) != 0 -> Z range
    uint32_t d = (gr.probeZ_u() - gr.cellOrgZ_u()) & 0xffffu;
    test = (b0 < d) ? 1u : 0u; // sltu(a0, d)
  } else {                     // X range
    uint32_t d = (gr.probeX_u() - gr.cellOrgX_u()) & 0xffffu;
    test = (aE < d) ? 1u : 0u; // sltu(a1, d)
  }
  if (test != 0) {
    c->r[4] = obj;
    c->r[5] = 1;
    psx::cpu::dispatchGuestToReturn0(*c, 0x80048fc4u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  }
  // ---- block 3: clamp the in-range coord, then recompute the other from it ----
  uint32_t lo = c->mem_r16(SP + 0x1AE), hi = c->mem_r16(SP + 0x1B0);
  if (lo < hi) {
    // Z branch: clamp 0x1C0 into [0x1AC, 0x1AC + 0x1B0], then recompute 0x1BC
    int32_t a2 = gr.probeZ();
    int32_t a1 = gr.cellOrgZ();
    uint32_t v1u = gr.cellOrgZ_u();
    if (a2 < a1) {
      gr.setProbeZ((uint16_t)v1u);
    } else {
      uint32_t a0u = c->mem_r16(SP + 0x1B0);
      if ((int32_t)((uint32_t)a1 + a0u) < a2) {
        gr.setProbeZ((uint16_t)(v1u + a0u));
      }
    }
    int32_t cv = gr.probeZ();
    uint32_t cb = c->mem_r16(SP + 0x1B4);
    int32_t pit = c->mem_r16s(SP + 0x1BA);
    int32_t prod = (int32_t)((uint32_t)((uint32_t)cv - cb) * (uint32_t)pit); // lo(mult)
    int32_t v = prod >> 14;
    gr.setProbeX((uint16_t)(gr.segX0_u() + (uint32_t)v));
  } else {
    // X branch: clamp 0x1BC into [0x1AA, 0x1AA + 0x1AE], then recompute 0x1C0
    int32_t a2 = gr.probeX();
    int32_t a1 = gr.cellOrgX();
    uint32_t v1u = gr.cellOrgX_u();
    if (a2 < a1) {
      gr.setProbeX((uint16_t)v1u);
    } else {
      uint32_t a0u = c->mem_r16(SP + 0x1AE);
      if ((int32_t)((uint32_t)a1 + a0u) < a2) {
        gr.setProbeX((uint16_t)(v1u + a0u));
      }
    }
    int32_t cv = gr.probeX();
    uint32_t cb = gr.segX0_u();
    int32_t pit = c->mem_r16s(SP + 0x1BA);
    int32_t prod = (int32_t)((uint32_t)((uint32_t)cv - cb) * (uint32_t)pit); // lo(mult)
    int32_t v = prod >> 14;
    gr.setProbeZ((uint16_t)(c->mem_r16(SP + 0x1B4) + (uint32_t)v));
  }
}

void Collision::gridStep(uint32_t obj) {
  Core *c = this->core;
  c->r[4] = obj; // taxi-in for the verify super-call
  static int s_v = -1;
  if (s_v < 0) {
    s_v = cfg_dbg("gridstep") ? 1 : 0;
  }
  if (!s_v) {
    grid_step_4798c(c, obj);
    return;
  }
  static uint8_t *ram0 = (uint8_t *)malloc(0x200000);
  static uint8_t *ramN = (uint8_t *)malloc(0x200000);
  uint8_t spad0[0x400], spadN[0x400];
  uint32_t regs0[32];
  memcpy(regs0, c->r, sizeof regs0);
  memcpy(ram0, c->ram, 0x200000);
  memcpy(spad0, c->scratch, 0x400);
  grid_step_4798c(c, obj);
  memcpy(ramN, c->ram, 0x200000);
  memcpy(spadN, c->scratch, 0x400);
  memcpy(c->ram, ram0, 0x200000);
  memcpy(c->scratch, spad0, 0x400);
  memcpy(c->r, regs0, sizeof regs0);
  psx::cpu::callOriginalToReturn(*c, 0x8004798Cu, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  uint32_t sp = regs0[29] & 0x1FFFFFu, flo = (sp >= 0x800) ? sp - 0x800 : 0;
  int ro = -1;
  for (uint32_t a = 0; a < 0x200000; a++) {
    if (c->ram[a] != ramN[a] && !(a >= flo && a < sp)) {
      ro = (int)a;
      break;
    }
  }
  int so = -1;
  for (uint32_t a = 0; a < 0x400; a++) {
    if (c->scratch[a] != spadN[a]) {
      so = (int)a;
      break;
    }
  }
  static long ng = 0, nb = 0;
  if (ro >= 0 || so >= 0) {
    if (nb++ < 40) {
      cfg_logi("gridstep", "MISMATCH obj=%08x ram@%x spad@%x sp=%x", obj, ro, so, sp);
    }
  } else if (++ng % 2000 == 0) {
    cfg_logi("gridstep", "%ld matches", ng);
  }
}

// FUN_8004766C — Collision::snapObjectToTerrain. THE object-level entry point of the grid family:
// "put this actor down on the ground where it is standing."
//
// WHAT IT DOES IN GAME TERMS. Tomba! 2's field levels are 2.5D: the world is 3D, but everything that
// walks is constrained to a chain of PATH SECTORS, each a strip of collision cells with a gradient.
// This function takes an actor's current world position, drops it onto that path, and writes the
// corrected position back into the actor:
//   1. Seed the shared grid probe (GR_PROBE_X/Y/Z) from the actor's world position.
//   2. Clamp the probe onto the actor's current path sector (FUN_80047778) — that call also switches
//      the actor to a neighbouring sector, or picks a sector from scratch, when the probe has walked
//      off the end of the current one.
//   3. Load that sector's cell rows (gridSetup) and find the cell under the probe (gridQuery). If the
//      query finds nothing the actor is off the grid: RETURN 0 AND LEAVE ITS POSITION UNTOUCHED.
//   4. If the resolved cell is tagged CELL_SECTOR_LINK it is a hand-off cell: adopt the sector named
//      in the tag's low byte and go round again. This is how an actor walks between sectors.
//   5. On a terminal cell: fold the probe into slope-local deltas (slopeLocalB) and pick the floor
//      line under it (floorPick, which latches GR_BEST_LINE for the caller).
//   6. Displace the probe by that slope-local correction and store the result BOTH back into the
//      scratchpad probe and into the actor's world X/Z. Y is left alone — this call resolves the
//      GROUND PLANE position; height is applied by whoever consumes GR_BEST_LINE.
// Returns FUN_80047778's status (nonzero = the clamp settled), or 0 when the query found no cell.
//
// IDENTIFICATION EVIDENCE (why this name, not a guess about shape):
//   * Callers. Object-spawn inits call it as the "stand this thing on the ground" step, always on a
//     freshly positioned node and always paired with FUN_80048750: beh_sine_motion_sfx
//     (node[0x29]=0, node[0]|=1, FUN_8004766C(node), FUN_80048750(node)), beh_pad_child_linker,
//     beh_seaside_prox_substate::subB, beh_a08_scene_actor state-0 case-0xA, actor_zoned_attacker.
//   * The sibling. Ghidra headless (the Ghidra evidence workflow, scratch/decomp/grid_766c.c) shows FUN_80048750
//     is this function with the same seed + same resolve loop, but a different tail: it computes the
//     surface NORMAL of the picked line (ratan2/sqrt into 0x1F8001A0/A2). So the pair is
//     "snap onto the ground" + "read the ground's slope" — which is exactly what a spawn init needs.
//   * The sector machinery. FUN_80047778's own callees (decompiled likewise) name themselves:
//     FUN_800490E4 scans every sector in the table at 0x1F8001C8 for the one whose X/Z bounds contain
//     the probe and writes its index to ACT_SECTOR_ID; FUN_80048FC4 hands off to the previous/next
//     sector link (0x1F8001FC/FD) when the probe runs past a segment endpoint. ACT_SECTOR_ID is a
//     path-sector id, not a "tile type".
//   * Not PlatformHle material: no IRQ spin, no completion-flag poll, no hardware register — pure
//     control flow over scratchpad + object memory (checked against game/core/libapi_intr.cpp's
//     banner criteria before porting).
//
// READY-FRAME function: the guest-visible behavior descends sp by 32 and spills s0/s1/ra/s2 (r16/r17/r31/r18) at
// sp+16/+20/+28/+24 with their LIVE incoming values, restoring them before return. The port MIRRORS
// that guest frame exactly — the spilled bytes are guest state that SBS compares (see
// docs/faithful-execution.md and Collision::flatNormal above). The five `c->r[31] = 0x800476..u`
// stores are the guest return-address constants the callees spill into their OWN frames, so they
// stay literals (that is also what tools/dynamic differential evidence matches on).
// ORACLE: guest 0x8004766C
uint32_t Collision::snapObjectToTerrain(uint32_t obj) {
  Core *c = this->core;
  GridRay gr{c};
  ProbeActor act{c, obj};
  c->r[4] = obj; // a0 = the probe actor (guest ABI)

  // --- open + populate the mirrored guest frame (32 bytes; abi_extract --contract 8004766C) ---
  c->r[29] = c->r[29] + (uint32_t)-32;    // addiu sp,-0x20
  c->mem_w32(c->r[29] + FR_S0, c->r[16]); // sw s0,0x10(sp) — LIVE incoming s0
  c->r[16] = obj;                         // s0 = the probe actor, live across the calls
  c->mem_w32(c->r[29] + FR_S1, c->r[17]); // sw s1,0x14(sp) — LIVE incoming s1
  c->r[17] = GR;                          // s1 = GridRay base, live across the calls
  c->mem_w32(c->r[29] + FR_RA, c->r[31]); // sw ra,0x1c(sp)
  c->mem_w32(c->r[29] + FR_S2, c->r[18]); // sw s2,0x18(sp) — LIVE incoming s2

  // --- 1. seed the shared probe from the actor's world position ---
  // NB the STORES below are spelled as explicit guest-memory writes against the named GR_* offsets
  // rather than through GridRay's setters, matching the other five gated methods in this file:
  // tools/dynamic differential evidence compares the method body's guest-store sequence TEXTUALLY, so a store hidden
  // behind a lens setter is invisible to it and the method fails the gate for a reason that has
  // nothing to do with its behaviour. Reads stay on the lens. (Tooling limitation, reported with
  // this port — not a code smell.)
  c->mem_w16(GR + GR_PROBE_X, act.worldX());
  c->mem_w16(GR + GR_PROBE_Y, act.worldY());
  c->mem_w16(GR + GR_PROBE_Z, act.worldZ());

  uint32_t stepStatus = 0; // s2 — FUN_80047778's status; the return value once a cell resolves
  bool onGrid = false;     // did the walk settle on a terminal cell?
  for (;;) {
    // --- 2. clamp the probe onto the actor's current path sector ---
    c->r[31] = 0x800476B4u; // ra for FUN_80047778's own frame
    c->r[4] = obj;
    psx::cpu::dispatchGuestToReturn0(*c, 0x80047778u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
    const uint32_t clampStatus = c->r[2];

    // --- 3. load that sector's cell rows, then find the cell under the probe ---
    c->r[4] = act.sectorId(); // a0 = sector id (re-read: step 2 may move it)
    c->r[31] = 0x800476C0u;   // ra for FUN_80049968
    c->r[18] = clampStatus;   // s2 holds the status across both calls
    psx::cpu::dispatchGuestToReturn0(
        *c, 0x80049968u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__); // Collision::gridSetup(sectorId)
    stepStatus = clampStatus;
    c->r[31] = 0x800476C8u; // ra for FUN_80047CBC
    psx::cpu::dispatchGuestToReturn0(
        *c, 0x80047CBCu, psx::cpu::ExecutionBudget::currentTurn(*c), __func__); // Collision::gridQuery()
    if (c->r[2] == 0) {
      break; // off grid — actor position stays untouched
    }

    // --- 4. a hand-off cell moves the actor to another sector; re-resolve there ---
    const uint32_t cellRec = gr.cellRec();
    if ((gr.cellTag(cellRec) & CELL_SECTOR_LINK) == 0) {
      onGrid = true;
      break;
    }
    c->mem_w8(obj + ACT_SECTOR_ID, gr.cellLinkSector(cellRec));
    // The guest re-loads the cursor and re-tests the same bit here. Nothing between can have changed
    // it, so this second test always agrees with the first — kept because it is the guest's shape.
    if ((gr.cellTag(gr.cellRec()) & CELL_SECTOR_LINK) == 0) {
      onGrid = true;
      break;
    }
  }

  if (onGrid) {
    // --- 5. fold the probe into slope-local deltas, then pick the floor line under it ---
    c->r[31] = 0x8004771Cu; // ra for FUN_80048134
    psx::cpu::dispatchGuestToReturn0(
        *c, 0x80048134u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__); // guest slope-local delta
    c->r[31] = 0x80047724u;                                                     // ra for FUN_80048034
    psx::cpu::dispatchGuestToReturn0(
        *c, 0x80048034u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__); // guest floor selection

    // --- 6. displace the probe by the slope-local correction; publish it to probe AND actor ---
    const uint16_t snappedX = (uint16_t)(gr.probeX_u() + gr.localX_u());
    const uint16_t snappedZ = (uint16_t)(gr.probeZ_u() + gr.localZ_u());
    c->mem_w16(GR + GR_PROBE_X, snappedX);
    c->mem_w16(GR + GR_PROBE_Z, snappedZ);
    c->mem_w16(obj + ACT_WORLD_X, snappedX);
    c->mem_w16(obj + ACT_WORLD_Z, snappedZ);
  }

  // --- close the mirrored guest frame ---
  c->r[31] = c->mem_r32(c->r[29] + FR_RA); // lw ra,0x1c(sp)
  c->r[18] = c->mem_r32(c->r[29] + FR_S2); // lw s2,0x18(sp)
  c->r[17] = c->mem_r32(c->r[29] + FR_S1); // lw s1,0x14(sp)
  c->r[16] = c->mem_r32(c->r[29] + FR_S0); // lw s0,0x10(sp)
  c->r[29] = c->r[29] + (uint32_t)32;      // addiu sp,0x20
  return onGrid ? stepStatus : 0u;
}

// eov_* wrappers — guest-ABI adapters (args in c->r[4..], return in c->r[2]). One per leaf.
static void eov_collisionSnapObjectToTerrain(Core *c) {
  c->r[2] = eng(c).collision.snapObjectToTerrain(c->r[4]);
}

void Collision::registerOverrides() {
  tomba::native::declareOverride(0x8004766Cu, "Collision::snapObjectToTerrain", eov_collisionSnapObjectToTerrain);
}
