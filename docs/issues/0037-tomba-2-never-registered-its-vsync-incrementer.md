# 0037 — Tomba! 2 never registered its VSync incrementer

Status: fixed.

## Observed
Every frame from f562 logged `[frame-cadence:error] ... the guest's own vblank callback left its dwell counter
at 0 against a quota of 2` (2,382 lines in a 75 s real-window run, 2026-10-16). In gameplay the VSync callback
table `0x800ABDC0` read all zero, slot 4 included.

## Cause
`game/core/entry/runtime.cpp:Runtime::bootInit` recomposes FUN_80050b08's init prefix and stopped after the task
create at 0x80050BF0; the retail prefix then calls `VSyncCallback(0x800506B4)` at 0x80050BFC
(`jal 0x80085BB0`), which puts the dwell-counter incrementer LAB_800506B4 into slot 4. The `FrameCadence`
banner credited libsnd's SsSetTickMode with that registration, and named 0x80050BA8.

## Resolution
`FrameCadence::installIncrementer` makes that call; `bootInit` runs it after the task create, in retail order.
After: slot 4 holds `0x800506B4`, the cadence report reads `guest dwell counter 2 -> gate OPEN`, and the run logs
no frame-cadence error. Test: `tests/test_authentic_vsync_incrementer.cpp` (MAIN.EXE path and SHA-256 as
arguments) fails with the call removed and passes with it: slot 4 registered, two VBlank walks open the gate.
