# 0054 — areas 0, 6, 8, 9, 14 and 21 show in-betweens off halfway

Features: F-FPS60.

Status: open (2026-11-04). Areas 0, 8 and 9 traced (below); area 6 owned with one remainder; 14 and 21 not yet
traced.

## Reproduction
`warp <area>/0` from free roam, 330 frames, then debug `fps60dump` for 30 frames; unpaced, `ires=0`, `fps60=1`,
`PSXPORT_PRESENT_SINK=3840x2160`. Share = an in-between's distance toward the next real present over the pixels
the two reals differ in (`scratch/tomba2d/tools/area_shares.py`); halfway is 0.35-0.65.

## Observed (`scratch/tomba2d/walk2/reduced/shares.txt`, `verdicts.txt`)
| warp | in-betweens | pattern |
|---|---|---|
| `warp 0/0` | 31 | whole (1.00) every other interval for the first ten, then halfway; the motion is a few small sprites (100-200 px at 960x540) |
| `warp 6/0` | 32 | scattered 0.23-0.76; 9 off halfway (writers 0x80141A94 and 0x80129AF8 are unowned) |
| `warp 8/0` | 32 | alternates ~0.50 and 0.66-0.82 |
| `warp 9/0` | 17 | alternates 1.00 and 0.43-0.64: every other in-between is whole |
| `warp 14/0` | 32 | alternates ~0.50 and 0.58-0.65 |
| `warp 21/0` | 31 | scattered 0.33-0.75; 6 off halfway (an intro with a letterbox, not free roam) |

An alternation that repeats every other interval is a draw that moves on every second guest frame only, or a
producer that steps on alternate frames. In every area of the walk the first dumped in-between is whole (0.92-1.00);
in a separate run area 21's first one is not (0.64), so that first sample is not judged.

## Findings (`scratch/tomba2e/`)
- Display (`w3/reduced/display60.txt`, the 22-area walk with the 60.00 Hz display simulated from `presentseq`): every
  area shows each present once except one repeat (the 59.826 Hz field drift), with no skipped present, no backward
  step and every shown step half a logic frame. Areas 6, 8, 9, 14 and 21 compose every interval; their shares are
  6 0.23-0.76, 8 0.44-0.82, 9 0.43-0.64 with every second one whole, 14 0.48-0.63, 21 0.33-0.70.
- Area 0 (`w3/reduced/`): no motion is shown whole. Tomba's idle pose advances every second
  guest frame, so the intervals alternate between his move (0.42-0.59) and near-still ones, where the share counts
  three things:
  - The whole 24x24 cells at (264-312, 168-192) in 960x540 are not a roof but A00's flame, a 16x16 billboard at
    native (64, 68) linked by `Render::billboardCompose1` 0x8003C2D4 through `Render::billboardEmit` 0x8003C8F4
    (store watch on its packet: U steps 0x91, 0xB1, 0xD1, one 0x20 flipbook frame each logic frame).
    `billboard_state.cpp:BillboardRender::render` moves the corners by t and keeps the later packet words, so each
    flame frame changes on the in-between before its real present: every frame still shows for two presents.
  - The 0.05 interval (f1961) is the gull's first frame: it exists only in the later record, so the in-between
    before it does not draw it (`a00_f1961_held.png`).
  - The whole cells at the right edge (912, 120-192) are single-pixel seams between the swing post and the tower
    whose winner differs between the two real frames themselves (`a00_right_pop.png`), not motion.
- Area 8: the pixels shown whole are the water, stepping every other interval (`base/`). Its texture scrolls by A08's
  u16 >> 4 word 0x80145A6E (`OverlayGt3Gt4`); a texel step every other frame would read this way, since a byte UV
  cannot sit half a texel over. Not yet confirmed by reading the word per frame.
- Area 9 (`w3/reduced/a09_f5281.png`, `v9/run.log`): every second in-between is whole over the wall glow (23,700
  pixels at 960x540 in f5281), not a scattered texel change. The glow samples CLUT (880,250), whose rows change every
  logic frame (`vkpix`): A09 FUN_8010A060 runs nine palette animations, each `LoadImage` 0x80081218 of one 16-colour
  row from its table at 0x8010E05C + frame * 0x20. A palette swap in VRAM has no halfway for a primitive producer,
  so the glow steps on the in-between before each real present. Tomba's motion is halfway (0.43-0.64).
- Area 8 confirmed (`scratch/tomba2f/w2/reduced/a08_water_word.txt`, a store watch on 0x80145A6C-0x80145A70): the
  water's word 0x80145A6E is rewritten every logic frame by A08's scenery GT4 0x801411D8 (from the walker,
  ra 0x8012A948). Its low byte U cycles 0x00, 0x40, 0x80 each frame and its high byte V steps 0x40 every third
  frame: a nine-frame 3x3 flipbook of 64-texel cells, not a scroll. A flipbook frame has no halfway, so the
  in-between takes the later cell (a held input) and the water shows each cell for two presents. A limit of the
  effect, not a missing producer; the rest of area 8 is halfway.
- Area 6 owned: the tile grid 0x80141A94 is `TileGridLayer::emitClamped` (rows clamp by 0x1F8000F0, nothing while
  0x800BF871 is 0x13) and the line 0x80129AF8 a `GuestKeyedEmitters` leaf keyed by call site. The journal is clean
  and every present composes, but the shares stay 0.23-0.76 (`scratch/tomba2f/w11/reduced/shares.txt`): the off
  pixels sit in (0-96, 192-336) at 960x540 (`scratch/tomba2f/w4/reduced/a06_left_f1978.png`), the idle leafy NPC in the
  left margin, its tiles alternating near 0.1 and 0.9 over a few hundred changed pixels. Not yet traced to its
  writer.
- Areas 12 and 14 read halfway from the first sample in the base run (`base/`).

## Next
Area 6: find which producer draws the left-margin NPC and why its in-betweens alternate. Then areas 14 and 21.
