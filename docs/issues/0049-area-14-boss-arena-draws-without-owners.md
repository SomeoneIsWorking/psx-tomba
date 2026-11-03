# 0049: the area 14 boss arena still has unowned movers

Features: F-FPS60, F-WIDE.

Status: arena geometry and waterfall owned (2026-11-03); the sprite and dialog movers are open. Owners:
`game/render/backdrop_plane.cpp` (`BackdropPlane`), `game/render/unlit_model_emitter.cpp` (`kA0eArena`); the open
movers are tracked with 0039 (unowned in-betweens).

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

## Open

The in-betweens on this screen are still whole-interval held, so they show the later real frame (measured: interp
vs next real MAE 0 at fences 1900, 1901). Two unowned movers hold them:

- 0x80027A4C (`ScaledSprites`, `cmd=0x2C`, OT 2046), presents 1499-2368, 773 intervals. Not from FUN_801104D0
  (the guest tail `BackdropPlane::draw` calls): a `GuestKeyedEmitters` tag of 0x801104D0 left the row in place. Its
  caller on this screen is not traced.
- 0x8007CDD4 (dispatched 0x8007DA50, `cmd=0x65`, OT 2): the dialog sprites, shared with 0039.

The fight itself was not reached in these runs; its rows (0x8003B320, 0x80029664, 0x8003BB50 block 0x801120E4) are
from the earlier journal and unconfirmed after this change.
