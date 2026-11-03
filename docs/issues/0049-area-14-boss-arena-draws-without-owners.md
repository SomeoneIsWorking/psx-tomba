# 0049: the area 14 boss arena still has unowned movers

Features: F-FPS60, F-WIDE.

Status: fixed (2026-11-03); the fight is unchecked. Owners:
`game/render/backdrop_plane.cpp` (`BackdropPlane`), `game/render/unlit_model_emitter.cpp` (`kA0eArena`),
`game/render/backdrop_sprites.cpp` (`BackdropSprites`), `game/ui/dialog_glyphs.cpp` (`DialogGlyphs::prompt`).

## Reproduction

Hidden, unpaced, ires 0, fps60 1, aspect 16:9, sink 3840x2160: autoskip to free roam, `warp 14`, run to the Evil Water
Pig's entrance dialog.

## Observed

With the sky bands owned (0045), the owner journal on `screen=warp 14` still reports
(`scratch/tomba2/evidence/0045_a14_owner_rows_after.txt`):

- `UnownedMoving` and `Overwrote` from A0E 0x80113AEC (GT3, `cmd=0x34`) and 0x80113DE0 (GT4, `cmd=0x3C`), both
  dispatched from 0x80114320, OT 269-1701: the arena geometry; the waterfall wall also stops at the right 4:3 edge
  (0028).
- `UnownedMoving` from 0x80027A4C (`ScaledSprites`, `cmd=0x2C`, OT 2046).
- `UnownedMoving` from 0x8003C048 (block 0x801110D4, dispatched 0x80110CA4, `cmd=0x3C`, OT 1640-2045).
- `UnownedMoving` from 0x8007CDD4 (dispatched 0x8007DA50, `cmd=0x65`, OT 2): the dialog sprites.

Each holds its interval: an in-between present repeats a real frame instead of blending.

## Fixed

- 0x80113AEC / 0x80113DE0 are `UnlitModelEmitter` gt3 / gt4 under the `kA0eArena` variant: SOP's body plus the hide of
  west records by model x while 0x1F80009C is set.
- 0x80110CA4 is `BackdropPlane::draw`: the 10x7 waterfall plane, its mirror below y 0 and the haze band, re-projected
  by a `ListStateProducer` under the blended view. Its screen gate is the draw window, so the wall fills 16:9
  (`scratch/tomba2b/b14i/w1100_small.png`). The 0x8003C048 row (block 0x801110D4) was this body's GT4 path.

Shown on `warp 14` with the same reproduction: the owner journal no longer reports 0x80113AEC, 0x80113DE0 or
0x8003C048 (`scratch/tomba2b/evidence/0049_a14_owner_rows_after.txt`).

## The sprite and dialog movers

The in-betweens were whole-interval held, showing the later real frame (interp vs next real MAE 0 at fences 1900,
1901), by two unowned movers:

- 0x80027A4C (`ScaledSprites`, `cmd=0x2C`, OT 2046): a watch on bucket 2046 put every link at ra 0x80111F98, inside
  A0E FUN_80111E14, which `GradientBackdrop::emit` calls after its bands. It seeds eight anchors (0x80126F4C) from
  0xFEDCB7 when 0x8011CE60 is set, projects each under the camera and stamps the sprite records 0x8011B8EC through
  `ScaledSprites` at bucket 0x7FE, scaled by MAC0. Now `BackdropSprites::draw`, a `ListStateProducer` that re-projects
  the anchors under the blended view.
- 0x8007CDD4 (`cmd=0x65`, OT 2): the blinking button prompt in the box's corner, drawn by FUN_8007D594 case 3. Now
  `DialogGlyphs::prompt`, keyed to the box like the glyphs.

Shown, same reproduction: the owner journal has no `UnownedMoving` row
(`scratch/tomba2c/evidence/0049_a14_owner_rows_after.txt`, through the entrance dialog with the prompt blinking,
`0049_a14_dialog_prompt_sheet.jpg`); bucket 2046 is written only by the native entry 0x80111E14 (ra 0x8011430C) and
the OT clear (`0049_a14_bucket2046_writers.txt`). Over the waterfall (crop 1400x700+900+100 of the 3840x2160 dump,
fences 1850-2149) the in-betweens blend: intervals without a texture-frame step sit at a mean share of 0.513 (n=119);
intervals where the clock 0x1F80017C steps the waterfall's texture frame read 0.80 (n=139), since a texture frame
cannot be halved (as the fire in 0046). Fence 1900 interp vs previous/next MAE 18.9/3.6, fence 1901 3.8/0.6
(`0049_a14_waterfall_shares.txt`).

## Open

The fight itself was not reached in these runs; its rows (0x8003B320, 0x80029664, 0x8003BB50 block 0x801120E4) are
from the earlier journal and unconfirmed.
