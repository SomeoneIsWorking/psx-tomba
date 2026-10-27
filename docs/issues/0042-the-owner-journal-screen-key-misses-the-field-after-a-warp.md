# 0042: the owner journal's screen key misses the field after a warp

Status: closed (2026-10-27). Owner: `game/core/debug/debug_menu.cpp` (`screen`).

## Observed
Tomba 2 through the title route and `warp 1`: the journal's rows after the warp carried route `warp 0 0` although
area 1 was running.

## Cause
The key read the warp destination word `0x800BF83A`, which is cleared once the area is entered.

## Fix
The key reads the current-area byte `FrameCut::kAreaByte` (`0x800BF870`) while the field runs; stages before GAME
route as `pick tomba2`. Shown in `scratch/redesign-review/t2field5`: the area-1 row carries key `0x8010637C00010001`
and route `warp 1`.
