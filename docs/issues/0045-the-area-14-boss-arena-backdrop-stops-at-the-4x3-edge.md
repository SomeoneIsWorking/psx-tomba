# 0045: the area 14 boss arena backdrop stops at the 4:3 edge

Features: F-WIDE.

Status: fixed (2026-11-03). Owner: `game/render/gradient_backdrop.cpp` (`GradientBackdrop::emit`).

## Reproduction

Hidden, unpaced, ires 0, fps60 1, aspect 16:9: autoskip to free roam, `warp 14`, a `pshot` 25 s later (the Evil
Water Pig's entrance dialog).

## Observed vs expected

The blue gradient sky behind the arena was drawn only over the 4:3 picture; both margins were black while the
arches, the floor and the actors drew across the 16:9 width. Expected: the sky fills the canvas.

## Cause

`otattr packets` names the four POLY_G4 packets at the far bucket (0x7FF) as written by A0E `FUN_80114320`'s
direct call `FUN_80114178`. That body writes literal x 0 and 0x140 into each band's four vertices, so the bands
end at the retail 320 columns whatever the canvas.

## Fix

`GradientBackdrop::emit` owns A0E `FUN_80114178`: the same four bands, colours, camera-pitch rows, OT links,
SetDrawMode packet and call to `FUN_80111E14`, with the columns taken from `wide_window::drawWindow`. At 4:3 the
window is `[0, 320)`, so the packets are retail's. The body exists only in A0E.

The native body first wrote its bands with no object scope, so the owner journal named them
`UnownedMoving guest fn=0x80114178 ... ot=0:2047-2047 cmd=0x38` on `screen=warp 14`: while the camera pitch moved, the
sky held the whole interval between frames. `GradientBackdrop::emit` now writes the bands and their draw mode under one
object scope, saves the pitch row, columns and OT slot, and its state producer re-emits the four bands at the pitch row
moved by t (`docs/producers.md`). The scope closes before the call to `FUN_80111E14`.

## Shown

Hidden, unpaced, ires 0, fps60 1, aspect 16:9, sink 3840x2160: the dialog frame after `warp 14`, the sky across the
16:9 width (`scratch/tomba2/evidence/0045_a14_producer_4k_small.png`). The owner rows on `screen=warp 14` before the
producer (`0045_a14_owner_rows_before.txt`) include `fn=0x80114178`; after it (`0045_a14_owner_rows_after.txt`) they do
not. The first before/after pair (`0045_a14_before_after.png`) was taken at a 1920x1080 sink.

The other movers on this screen are open in 0049: A0E's GT3/GT4 pair 0x80113AEC/0x80113DE0 (also the waterfall wall
that still ends at the right 4:3 edge, `0045_a14_waterfall_4k_small.png`, listed in 0028), `ScaledSprites` 0x80027A4C
and 0x8003C048.
