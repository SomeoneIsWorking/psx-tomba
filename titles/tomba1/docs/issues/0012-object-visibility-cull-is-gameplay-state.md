# 0012: objects appear only within 64 columns of the 4:3 picture, and their cull is gameplay state

Status: open (2026-11-14), needs a decision. Owner: the resident object culls (`SCUS_942.36`), no port owner yet.

## Observed
At 21:9 the canvas reaches 120 columns past each side of the 320-column picture, but an object is listed for drawing
only while its anchor is within 64 columns of it (16:9: 54 columns, so only an object wider than 20 columns shows the
cut). In `warp 0/0` at 21:9 objects at 386, 400 and 406 columns from the camera's left edge are active and unlisted
inside the canvas (`scratch/t1ws/objscan.py` over `scratch/t1ws/ob21`).

## Found (2026-11-14)
Names and matches from tombi-decomp (`config/scus94236/symbol_addrs_scus94236.txt`):
- `ObjCullRegister` `0x80022E44`: `+1` = 0 unless (x - `0x1F800176` + 0x40) < 0x1C1 and (`0x1F800186` - y + 0x40) < 0x171;
  then `+1` = 1 and the object is pushed on its category list (`0x1F800218`..`0x1F800230`, pushers `0x80018BC8`..). Area 4's
  overlay calls it 66 times.
- `cullObjectToDrawList` `0x80022C08` and `cullObjectToDrawListMargin` `0x80022D3C`: the same with a per-object margin
  (`+0x6C`); `isObjectInView` `0x80022F6C` and `isOnScreen` `0x80022FD8` the 64-column test without the list.
- The lists are walked by the draws (`drawCategory1..8Objects`) and by weapon collision
  (`collideWeaponsWithCat2/4/5Objects` `0x80053DA0`..`0x80054618`), and updates branch on `+1` (handlers that run only
  while it is set, placed objects that stop spawning when it is clear).
- `despawnObjectOffscreen` `0x80023020` and `despawnObjectOffscreenMargin` `0x800230BC` retire objects beyond 160
  columns; outside every canvas the aspects reach, so they never show.

## Decision needed
Widening the cull by the canvas margin, as Tomba! 2's cone cull does (root S005), draws the margin objects with their
real state but runs their updates and lets weapons hit them in the margin, so logic differs between 4:3 and widescreen.
Drawing them without listing them for logic would show objects whose updates have not run since they left the window
(frozen or never spawned). Keeping the guest cull leaves the 21:9 pop-in. Pick one; nothing is changed until then.
