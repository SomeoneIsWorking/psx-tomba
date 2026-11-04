# 0054 — areas 0, 6, 8, 9, 14 and 21 show in-betweens off halfway

Features: F-FPS60.

Status: open (2026-11-04). Owners: not traced; the owner journal holds no interval in these areas after arrival,
so the composed in-betweens come from producers that place a moving draw off halfway, or from draws without a
producer that the journal does not flag.

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

## Next
Per area, dump the owner journal rows for keyed producers whose t-placement differs from the composed one, starting
with area 0 (the earliest), then 9.
