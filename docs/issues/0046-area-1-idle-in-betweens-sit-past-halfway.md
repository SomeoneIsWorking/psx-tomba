# 0046: area 1 idle in-betweens sit past halfway toward the next frame

Features: F-60FPS.

Status: open; cause traced to psxport (2026-11-03). Owner: psxport `runtime/psx/present/blend.h` (`lerpInt`).

## Reproduction

Hidden, unpaced, `ires=0` (8x), `fps60=1`, sink 3840x2160: `tomba2_port scratch/bin/tomba2/SCUS_944.54` with
`PSXPORT_AUTO_SKIP=1`, `warp 1` once the field runs, presents dumped from fence 3000 to 3299 with Tomba standing
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

At the owner: `lerpInt` rounds ties half to even, so no tie favours either frame. It is a psxport change shared by
every title's blends, so it lands with psxport's all-title rebuild and real-time run; not applied from the Tomba lane.
