# 0046: area 1 idle in-betweens sit past halfway toward the next frame

Features: F-60FPS.

Status: open; cause not traced.

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

## Next step

Find the producer that draws Tomba's model in area 1 (`docs/producers.md` element-namer table) and check whether
its `render(from, to, t)` blends a word that is already the next frame's (pose, keyframe index or matrix) or
lerps a ring value the long way.
