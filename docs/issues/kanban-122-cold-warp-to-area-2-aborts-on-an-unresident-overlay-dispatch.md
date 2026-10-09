---
id: kanban-122
title: A cold warp to area 2 aborts in the area handler dispatch
status: closed
labels: [bug, field]
---

`warp 2` loaded the area (`warp: cold area 2 sub 0 loaded`) and the next field aborted in `Engine::submode1` case
4/5/6 dispatching the guest area handler `0x80107230` / `0x8010766C` / `0x80107790`:

```
[executor:error] submode1 required a completed guest call, but execution exited as budget-exhausted at 0x8009A438 after 564482 cycles
```

## Cause

Not an unresident overlay. The handlers live in the GAME stage image (`0x80106228`..), which is resident; the abort
was a cycle budget exit. `Engine::submode1` ran them under one display field (564,480 cycles), and the handler's
first state (`0x80107230`, sm[0x4e]==0) calls `FUN_8007B18C`, whose byte-wise `FUN_8009A420` (memset) loop clears
0x208 pool records, then the remaining init calls. Measured: the handler needs 695,994 cycles, 2 turns. Any
route into area state 4/5/6 aborts the same way; areas 2, 3, 7 and 20 map to it in the table at `0x80108F60`.

## Fix

`tomba::guest::dispatchHandlerToReturnResuming` (`game/core/overrides/guest_jal.h`) resumes the handler across host
turns through psxport's `callGuestToReturnResuming`; `Engine::submode1` uses it for the three cases.

## Test

`test_long_area_handler` (authenticated MAIN.EXE): a one-turn dispatch of `FUN_8007B18C` exits budget-exhausted,
the resuming dispatch completes and leaves the pool free count at 0x208.
