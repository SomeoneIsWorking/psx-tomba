# 0068 — Tomba's run bob steps on the tick

Features: F-FPS60.

Status: fixed (2026-11-14). Owner: `game/core/frame/draw_motion.cpp` (`DrawMotion`, the `tomba` track).

## Reproduction
Boot with `PSXPORT_AUTO_SKIP=1`, `warp 0/0`, hold right, then tap cross four times, 6 frames apart, and dump 48
presents; unpaced, `ires=0`, `fps60=1`, `PSXPORT_PRESENT_SINK=3840x2160` (`scratch/s006/anim2.py`, `anim3.py`).

## Observed vs expected
While Tomba runs, his hair's centroid on each in-between sits 4.6 to 8.8 sink pixels off the midpoint of its
neighbours, against real steps of 9 to 18 pixels (`scratch/s006/hair_a2.txt`). Often it sits on the newer present
(849.4, 867.0, 867.1). Expected: about 0 to 2 pixels, which is what his limbs and every other node show.

## Cause
`FUN_800597AC` builds Tomba's transforms inside the draw and adds the animation's root offset `G+0x88/8A/8C`, turned by
his body matrix, to his position (`external/tomba2-decomp/docs/notes/tomba-draw-transform.md`). `FUN_80075F0C` steps
`G+0x8A` every tick and `FUN_80076904` reloads it at each keyframe, so the running bob changes by 0xA to 0xE units a
tick. `game/core/frame/draw_motion.cpp:DrawMotion` tracked his position, angles and part angles but not that offset, so
the in-betweens drew the bob at the newer tick.

## Fix
The `tomba` track blends `G+0x88` as two s16 halves and `G+0x8C` as a low s16 (`kTombaFields`).

Shown (`scratch/s006/b3`, same plan and settings): in-betweens of the run sit 0.1 to 2.1 pixels off the midpoint
(`hair_b3.txt`), with the limbs halfway (`bob_after.png`, against `bob_before.png`). A store watch on the record angles
shows the replay writing the halfway angle of two absolute poses across keyframe loads and the jump's animation switch
(`0x800F27D0`: 0x1068 → 0x0D5 written as 0x109E), and his records' world positions sit within 2 units of the midpoint
of the two real draws except at the jump's take-off, where the blended angles follow the arc (`recy.py`, up to 12).
The pad replay `scratch/pause-replay/rec2/session.pad` run with fps60 off and on (`leak0`, `leak1`) leaves main RAM
and the scratchpad identical at all 1638 pad frames, 1580 intervals replayed, 0 refused.
