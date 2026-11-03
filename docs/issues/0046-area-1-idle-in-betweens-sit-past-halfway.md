# 0046: area 1 idle in-betweens sit past halfway toward the next frame

Features: F-60FPS.

Status: fixed (2026-11-03). Owners: psxport `runtime/psx/present/blend.h` (`lerpInt`), `game/render/quad_state.cpp`
(`lerpVertex`).

## Reproduction

Hidden, unpaced, `ires=0` (8x), `fps60=1`, sink 3840x2160: `tomba2_port scratch/bin/tomba2/SCUS_944.54` with
`PSXPORT_AUTO_SKIP=1`, `warp 1/0` once the field runs, presents dumped from fence 3000 to 3299 with Tomba standing
idle. Shares measured with psx-spyro's `scratch/filed-defects-2/0220/judge/sweep.py` (where an in-between sits
between the real presents either side, over the pixels that differ between them).

## Observed vs expected

Only Tomba's idle animation moves. Every one of the 299 in-betweens is composed at `t=0.5` (`[presentseq]`), yet
they sit 0.51-0.91 of the way toward the next real present, mean 0.71, 200 above 0.63
(`scratch/idle-share/shares.txt`, crops `tomba-triple.png`, diff `look.png`). Expected: about 0.5, as the
Crash Bash and Spyro 2 in-betweens measure with the same tool. A consistent lean toward the next pose is
possible judder: the motion steps short, then long.

## What the measure saw

The whole-frame pixel share mixes two things. The bottom-right fire changes its sprite frame on some intervals; a
frame of a texture animation cannot be halved, so those in-betweens show the later frame (share 1.00,
`scratch/tomba2b/evidence/0046_fire_frame_step.png`), while the fire sprite's vertices sit at 0.498. Over Tomba's crop
alone the pixel share is 0.56 (`0046_vertex_shares.txt`).

## Cause

Measured on the vertices instead of pixels: each Tomba sub-part list (`ListStateProducer::render`, lists
`0x8017C868`-`0x8017ECF4`) rendered at t=0, 0.5 and 1 and matched by texture coordinates lands at a vertex-weighted
0.565 (median 0.61) of a 0.6-native-pixel idle step. psxport `present/blend.h:lerpInt`, which
`blendGteControl` uses for the translation words CR5-7, rounds with `llround`: at t=0.5 an odd span is a tie, and half
away from zero always lands on the later frame's side. With the ties rounded half to even (a temporary build) the same
probe reads 0.503 (median 0.502). The title's producer is correct; the lean is 0.04 native pixels (0.3 output pixels at
8x).

## Fix

`lerpInt` rounds the blended value to the nearest integer with ties to even, so no tie favours either frame;
`lerpTurn` and this title's `lerpWrapped` (`list_state_producer.cpp`) round the absolute value the same way instead
of rounding the step alone. Values that are not ties are unchanged, so every other title's blends move only on ties.

Rounding each blended value on its own deforms a shape that moves whole: `lerpVertex` (`quad_state.cpp`) blended
each screen vertex of a saved packet separately, so at t=0.5 a primitive moving by an odd step lost or gained one
native pixel of span whenever its two ends had different parity (x0=10, x1=25, step 3: width 14 for 15). It now
carries the exact halfway place as the whole pixel and the fraction past it (`RecordVertex::subX/subY`, which the
raster setup places at the output scale), so every span of a rigid move is kept and each vertex sits at exactly
t. Texture coordinates have no fraction, so their step is rounded instead and equal steps stay equal.

## Shown

Same reproduction (`warp 1/0`, fences 2950-3100), the vertex probe as above (a temporary build, removed): Tomba's
sub-part lists land at a vertex-weighted 0.502, median 0.5015, over 4047 native pixels of motion
(`scratch/tomba2c/evidence/0046_vertex_shares_after.txt`). Other titles with the same pixel measure, hidden and
unpaced: Spyro 1 `warp 10` hub idle, fences 1500-1650, mean 0.505, median 0.505 (n=143,
`0046_spyro1_hub10_shares.txt`); Crash Bash `arena 1` rules screen, fences 2400-2600, mean 0.504, median 0.504
(n=200, `0046_crashbash_arena1_shares.txt`).

`lerpVertex`, same build settings, `warp 0/5`, `0/0` and `0/7` with a temporary probe in `lerpPrimitive` (removed):
388 saved-packet primitives moved rigidly by an odd step at t=0.5; with the half carried, every span equals the
real frames' span (largest error 0), where rounding each vertex would have changed 133 of them by one native pixel
(`scratch/tomba2c/evidence/0046_rigid_quad_spans.txt`). Boot, `warp 1/0` and `warp 14/0` blended no rigid odd step
through these packets.

Not judged: pacing on the simulated 60.00 Hz display. The host load stayed 16-50 over two passes, so no
real-time run was taken; it is owed with psxport's all-title real-time run.
