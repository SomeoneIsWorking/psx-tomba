# `FUN_800288AC` — the weapon IMPACT radial plume (mesh half)

> The display-time renderer (`Render::*Render`/`*Emit` host producers, `RenderQueue`, `pc_render`) named below was deleted; the picture is the guest's GP0 output on the GPU device. The guest RE stands as the reference for future producers.

**Status: decoded; runs as the guest body.** Decoded 2026-08-11 from ground truth `authenticated executable/overlay evidence`
`guest 0x800288AC` (lines 2410-2574, 164 raw lines, 9 of 13 blocks reachable — the other 4 are the
recorded binary evidence's folded-in trailing function) plus `guest 0x80027768` (the shared 36-byte-record mesh
writer). The missing writer parameter found by this decode is now modeled as
`meshQuadRecordsEmit`'s opt-in CLUT-row bias; `Render::impactPlumeRender` owns the controller from
persistent node/script state in `game/render/fx_impact.cpp`.

Prior art that must not be re-derived: this controller WAS ported once, as `FxMesh` in
`game/render/fx_mesh.cpp` (kanban #15, 2026-07-23), and that file was **deleted in `abf3cf9`** because
it derived its transform from `gte_read_ctrl(0..7)` — a tap, banned by `PROTOCOL.md`. The deletion was
correct. What follows resolves the same transform from NODE STATE instead, which is what makes a
display-pass producer legitimate (and interpolatable at fps60).

Reached both directly as a type-0x20 node render fn (`node+0x18 == 0x800288AC`) and through the
composite impact node `FUN_80033080 = { FUN_80027E5C(); FUN_800288AC(node); }`. The type-0x20 display
walk handles both routes. The sprite half renders through `impactBurstRender`; this mesh half is the
two `0x3E` gouraud quads that are the weapon-impact radial plume.

Live reachability, measured not assumed: `PSXPORT_GATE=1` pc_render + `PSXPORT_DEBUG=nofx,ringcensus`
on `replays/bugs/bucket-softlock.pad`, 460 frames headless, names `0x800288AC` (node `800EEBF8`) as
live-and-skipped in area 0. The original fix's repro is `replays/bugs/weapon-impact-bucket.pad`
**f654-660**, evidence bbox f656 `x[115,190) y[80,175)`.

---

## 1. The algorithm

```
FUN_800288AC(node):                      # a0 = node = r18;  0x1F800000 = scratchpad
  rec = *(u32*)(node + 0x3C)             # the ANIMATION SCRIPT cursor (4-byte records)
  if (rec == 0) return                   # no script -> emit nothing. The honest early-out.

  # --- object transform, entirely from the node's OWN state ---
  rotmat(&node[0x48], 0x1F800000)                 # FUN_80085480 = Math::rotmat, three Euler angles
                                                  #   node+0x48 / +0x4A / +0x4C
  colScale = { rec[0] << 2, rec[1] << 2, rec[2] << 2 }      # the SCRIPT's own scale triple
  matColScale(0x1F800000, colScale)               # FUN_80084520 = Math::matColScale

  # compose object rotation with the SCENE CAMERA rotation (camera CR0-4 from 0x1F8000F8),
  # three column MVMVAs (0x4A49E012 = mx=rot, v=IR, cv=none) writing back into 0x1F800000
  # translation: MVMVA (0x4A486012 = mx=rot, v=V0) on *(u32*)(node+0x2C) / *(u32*)(node+0x30),
  #              then ADD the camera translation at 0x1F80010C/110/114 -> 0x1F800014
  # publish composed rotation to CR0-4 and translation to CR5-7

  # --- the DEPTH CUE, driven from the script attribute byte ---
  attr = (u8)rec[3]
  if (attr & 0x40) {
      *(u32*)0x1F800090 = 4096 - ((attr & 0x3F) << 6)       # IR0: a REAL driven cue
      a1 = 0
  } else {
      *(u32*)0x1F800090 = 0                                  # IR0 = 0, cue is the identity
      a1 = attr & 0x0F                                       # see section 4
  }

  # --- advance the script, honouring the last-frame bit ---
  if (attr & 0x80) *(u32*)(node + 0x40) = 0                  # script ended
  else             *(u32*)(node + 0x40) = rec + 4            # next 4-byte record

  CR21 = CR22 = CR23 = 0                                     # far colour black: cue fades toward black
  FUN_80027768(a0 = *(u32*)(node + 0x50),                    # the mesh record list
               a1 = a1,                                      # section 4 — THE UNMODELLED ONE
               a2 = (s16)*(node + 0x32),                      # sort bias
               a3 = (u8)*(node + 0x29))                        # U offset
```

**It emits ONCE.** Contrast `FUN_8002BC9C` (the four-copy plume, ported as `Render::radialPlumeRender`),
which calls the writer four times advancing the Y angle a quarter turn between copies.

## 2. Node and script fields

| field | type | meaning |
|---|---|---|
| `node+0x29` | u8 | writer `a3` — U offset added to the four packet U bytes |
| `node+0x2C/0x30` | u32,u32 | world position, as the GTE `VXY0`/`VZ0` pair |
| `node+0x32` | s16 | the writer's sort bias (`MeshOtBias`) |
| `node+0x3C` | u32 | animation-script cursor, 4-byte records. **0 = emit nothing** |
| `node+0x40` | u32 | where the advanced cursor is stored back |
| `node+0x48/4A/4C` | s16 ×3 | the Euler angles `Math::rotmat` consumes |
| `node+0x50` | u32 | the mesh record list handed to the writer |
| `rec[0..2]` | u8 ×3 | column-scale triple, each `<< 2` |
| `rec[3]` | u8 | `attr`: bit7 = last frame, bit6 = drive the cue, bits5-0 = cue amount, bits3-0 = §4 |

## 3. What the writer does with each argument (`guest 0x80027768`)

| arg | treatment |
|---|---|
| `a1` (r5) | `r5 << 22`, then ADDED to the record's word0 before it is stored into the packet |
| `a2` (r6) | `r6 << 16` — the sort-bias half, already owned by `MeshOtBias` (`mesh_quads.h`) |
| `a3` (r7) | added to FOUR packet U bytes by read-modify-write **on the PACKET, not the record** — so the record is left clean and a read-only native producer MUST apply this offset itself. It maps exactly onto `meshQuadRecordsEmit`'s `uBias`, which adds to the record's U at draw time |

## 4. The required `a1` CLUT-row bias

`Render::meshQuadRecordsEmit` (`game/render/mesh_quads.cpp:131`) takes the CLUT straight from the
record: `const uint16_t clut = (uint16_t)(w0 >> 16);` (line 182). The guest instead adds `a1 << 22` to
that same word0 before building the packet. Bit 22 is **bit 6 of the `w0 >> 16` CLUT field**, and in a
PSX texpage/CLUT word the CLUT's low 6 bits are its X and the bits above are its Y — so `a1` shifts the
**CLUT ROW**, i.e. it selects a different palette. `a1 = attr & 0x0F` comes from the script, so the
effect is authored to change palette per script frame, and it is only non-zero on the branch where the
depth cue is the identity — the two mechanisms are alternatives, which is consistent with both being
ways of recolouring the same quads.

A producer that ignores `a1` draws the plume in the wrong palette on every record whose
`attr & 0x0F != 0`, and — this is the part that makes ignoring it unacceptable rather than approximate —
it would do so SILENTLY, since nothing compares native CLUT against guest CLUT.

The additive shared-writer change is now landed: `meshQuadRecordsEmit` takes an opt-in CLUT-row bias,
defaulted to zero so existing callers remain bit-identical. That follows `MeshOtBias`'s discipline:
only a controller that has RE'd the argument supplies it. It deliberately remains a CLUT-row
parameter rather than a vague packet-word bias.

Completed sequence:
1. `meshQuadRecordsEmit` gained the opt-in CLUT-row bias without changing defaulted callers;
2. `Render::impactPlumeRender` now rebuilds §1 from node state and is dispatched for direct
   `0x800288AC` nodes and composite `0x80033080` impact nodes;
3. the tracked impact replay was compared before/after in one deterministic SBS software-oracle
   route. Pane B remained byte-identical at all eight sampled frames. Native A gained 588/826/492/206
   pixels at f652/f654/f656/f658, confined to the producer's reported box; the blue-white plume is
   visible in both A and B at f656. Evidence: `scratch/logs/c15_{pre,post}.log` and
   `scratch/screenshots/c15_{pre,post}_f*_{A,B}.ppm`.

The direct-render-function route is positive-controlled separately by `bucket-softlock.pad`: the
final producer reports live `0x800288AC` nodes from f298 through f332, including both two-quad impact
meshes and eight-quad `0x800A0B38` meshes with non-zero U scroll. Evidence:
`scratch/logs/c15_standalone.log`.

## 5. Traps already paid for once — carried forward from kanban #15

- **`drawWorldQuad` sets `has_xyf`**, which makes fps60 skip a guest-execution-time prim at present: the
  effect is submitted every frame and yet zero pixels change. A display-pass producer is the shape that
  avoids this; do not re-diagnose it as a missing emission.
- **The burst needs `node+0x32`'s sort bias applied to depth or the bucket occludes it.** `MeshOtBias`
  exists for exactly this and takes the bias explicitly.
- **The attack button is CIRCLE, not SQUARE.** Every "not reproduced" verdict on kanban #15 before
  2026-07-23 held square and was therefore measuring nothing.
