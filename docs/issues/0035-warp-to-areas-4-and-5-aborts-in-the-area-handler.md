# 0035 — warp to area 4 or 5 aborts in the area handler

Features: F-DEBUG.

Status: fixed (2026-11-04). Owner: `game/object/behavior_dispatch.cpp` (`BehaviorDispatch::dispatchObj`).

## Observed
`warp 4/0` aborted with `dispatchObj required a completed guest call, but execution exited as budget-exhausted
at 0x80117CDC` (`scratch/tomba2d/walk_first/run.log`); area 5 aborted the same way at 0x8013500C.

## Cause
`dispatchObj` ran a guest object handler with `callGuestNow`, one segment of one field's cycle budget. A04's
area init FUN_801169E8 → FUN_80117E18 re-tints 2464 CLUT rows through FUN_80117C58, about five fields of guest
work.

## Fix
`dispatchObj` runs a guest handler with `dispatchHandlerToReturnResuming` (psxport `callGuestToReturnResuming`):
segments run back to back inside the one call until the handler returns, at most `kDefaultCallTurns` (8). A
segment ends on its cycle budget or on a guest VSync wait; either is continued at once in the same host frame,
with nothing presented between segments. This holds for every guest object handler, not only A04's: a handler
that waits on VSync now finishes in one host frame (load removal) where it used to abort.

Audit: the framework logs every call that needed a second segment against all completed calls. The walk through
every area's entry 0 logs two: A04 FUN_801169E8 (5 segments) and A05 FUN_80112020 (4), both from GAME's
dispatcher (return 0x801087DC), of 93,748 calls (`scratch/tomba2d/walk/run.log`).

## Shown
`warp 4/0` logs four `dispatchObj suspended after turn N` lines within 20 ms of one host frame, then lands in the
snowy Ranch Area (`scratch/tomba2d/a4/run.log`, `scratch/tomba2d/a4/strip.jpg`); `warp 5/0` lands in the same
run.
