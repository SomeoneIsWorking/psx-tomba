# 0054 — areas 0, 6, 8, 9, 14 and 21 show in-betweens off halfway

Features: F-FPS60.

Status: open (2026-11-04). Areas 0, 8 and 9 are not judder (below); area 6 waits on two unowned writers; 14 and
21 not yet traced.

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
- Area 0 (`a0/reduced/`): no motion is shown whole. Tomba's idle pose advances every second guest frame, so the
  intervals alternate between his move (interpolated, `tombadiff_small.jpg`) and near-still ones. In a still interval
  the only changed pixels are a gouraud colour step on the orange roof behind the fence, polygon pops by the right
  tower and the gull's first appearance; the share metric reads those as whole. No code change.
- Area 8: the pixels shown whole are the water, stepping every other interval (`base/`). Its texture scrolls by A08's
  u16 >> 4 word 0x80145A6E (`OverlayGt3Gt4`); a texel step every other frame would read this way, since a byte UV
  cannot sit half a texel over. Not yet confirmed by reading the word per frame.
- Area 9 (`a9/reduced/`): the whole in-betweens change only scattered texels inside the wall glow (824 pixels, no
  geometry; the journal is clean), a texture content change; Tomba's motion is halfway (0.43-0.64).
- Area 6: A06's tile grid 0x80141A94 is unowned. It is not a `TileGridLayer` copy: its rows clamp to the map's first
  or last row by the camera's direction (0x1F8000F0) instead of wrapping, and it draws nothing while 0x800BF871 is
  0x13. Also unowned: 0x80129AF8 (GP0 0x5A).
- Areas 12 and 14 read halfway from the first sample in the base run (`base/`).

## Next
Area 6: port 0x80141A94 as a `TileGridLayer` walk with clamped rows, then 0x80129AF8. Then areas 14 and 21.
