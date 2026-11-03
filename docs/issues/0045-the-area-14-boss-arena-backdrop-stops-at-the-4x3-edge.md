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

## Shown

Same route, 1920x1080 sink, the dialog frame before (top) and after (bottom):
`scratch/tomba2/evidence/0045_a14_before_after.png`. At 3840x2160 later in the cutscene the gradient fills the left
margin (`0045_a14_waterfall_4k_small.png`); the waterfall wall still ends at the right 4:3 edge, which is A0E's
GT3/GT4 pair 0x80113AEC/0x80113DE0 listed open in 0028.
