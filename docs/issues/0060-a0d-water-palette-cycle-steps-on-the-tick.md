# 0060 — A0D's water colour cycle steps on the tick at 60 fps

Features: F-FPS60.

Status: fixed (2026-11-11) for replayed in-betweens; a composed in-between (menus, pause) keeps the newer entries.

## Reproduction
`warp 13/0` from a paused boot stepped to 1050, step 1800 (the boss intro with the view still), unpaced, `ires=0`,
`fps60=1`, sink 3840x2160 (`scratch/wave0058/wave_plan.py`, run `h1`). The water left of the bridge
(sink x 0..480, y 1100..1700).

## Observed
The water's mean change between consecutive presents is 3.9, 1.2, 1.1, 8.4, 2.3, 0, 0, 8.1, 0, 2.5, 2.4: the small
halfway steps are the screen wave (fixed in 6c11339), and every other tick a large change lands whole on one
present; on in-between 8 every one of the 216 moving 32 px tiles equals the following real (`whole.py`,
`greens.py`, `h1_whole.png`).

## Cause
A palette cycle, not geometry: VRAM row y 250, x 816..821 (the water's CLUT entries 4..9) rotates one entry every
other tick (`scratch/wave0058/v1`, raw VRAM per frame with fps60 off), uploaded in the update from a doubled colour
table in A0D near `0x80117580` (the writer is not found statically). An in-between is spliced onto the newer record
and `psxport runtime/psx/present/record_cluts.cpp:RecordCluts::resolve` gives its primitives that record's CLUT
entries, so the rotation shows on the in-between before the tick it belongs to.

## Fix
`RecordCluts::index(shown, {earlier, t})` (psxport `runtime/psx/present/record_cluts.cpp`): a CLUT the in-between's
primitives sample that both records sampled (same VRAM position, same depth) with different entries resolves to
their blend at t = k/n, each 5-bit channel by `lerpInt`, the STP bit from the newer tick, 0x0000 (the transparent
texel) never blended. The cycle is a rotation, so entry i goes from colour c[i] to c[i+1]; blending each entry gives
the halfway colour between the two shown entries, which is what the in-between should show between two real frames.
No rotation is guessed: the blend works on the two real tables only. A cycle that steps every other tick still
changes over two presents of four; spreading it over its period would need the cycle's owner and is not done.

## Evidence
Run `h3` (fix, fps60 on) and `h4` (fps60 off), same plan. Water box mean change per present: 5.6, 1.2, 1.1, 5.1,
5.5, 0, 0, 3.8, 4.3, 2.5, 2.4 (before: the 8.4 and 8.1 steps landed on one present). At in-between 8, 0 of 216 moving
tiles equal the newer real (before: 216); its mean distance from the midpoint of its neighbours is 1.5 against 5.7
and 6.3 from each (the wave's motion is the rest). `h3_water_7_8_9.png`: the middle crop is the cross-fade. The real
presents of `h3` are byte-identical to `h4`'s, and guest RAM at the two marks is identical with fps60 on and off.
