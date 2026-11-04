# 0035 — warp to area 4 or 5 aborts in the area handler

Features: F-DEBUG.

Status: fixed (2026-11-04). Owner: `game/object/behavior_dispatch.cpp` (`BehaviorDispatch::dispatchObj`).

## Observed
`warp 4/0` loaded the area and aborted with `dispatchObj required a completed guest call, but execution exited as
budget-exhausted at 0x80117CDC after 566206 cycles` (`scratch/tomba2d/walk_first/run.log`); area 5 aborted the same
way at 0x8013500C before.

## Cause
`dispatchObj` ran a guest object handler in one host turn. A04's area init FUN_801169E8 calls FUN_80117E18, which,
while bit 0x10 of 0x800BFE56 is clear, re-tints 2464 CLUT rows through FUN_80117C58 (StoreImage, DrawSync, LoadImage
per row). That is about five fields of guest work, past one turn's cycle budget.

## Fix
`dispatchObj` runs a guest handler with `dispatchHandlerToReturnResuming`, which resumes the call one field per host
turn until it returns, the way the console spends those fields.

## Shown
`warp 4/0` logs `dispatchObj suspended after turn 1..4` and lands in the snowy Ranch Area with the HUD
(`scratch/tomba2d/a4/run.log`, `scratch/tomba2d/a4/strip.jpg`); `warp 5/0` lands in the same run. The walk through
every area's entry 0 lands all 22 with no FATAL (`scratch/tomba2d/walk/drive.log`, `walk/reduced/sheet0.jpg`,
`sheet1.jpg`).
