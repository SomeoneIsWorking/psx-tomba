# 0049: the area 14 boss arena still has unowned movers

Features: F-FPS60, F-WIDE.

Status: open. Owners: the A0E overlay draw bodies below; tracked with 0028 (margins) and 0039 (unowned in-betweens).

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

Each holds its interval: on the in-between presents those draws stay at the earlier frame.

## Next step

Producers for the A0E GT3/GT4 pair first (largest share of the picture), then the sprite paths shared with 0039.
