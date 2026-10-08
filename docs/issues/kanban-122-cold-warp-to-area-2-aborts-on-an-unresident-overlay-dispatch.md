---
id: kanban-122
title: A cold warp to area 2 aborts on an unresident overlay dispatch
status: open
labels: [bug, field]
---

`tomba::applyColdWarp` (`game/core/debug/dev_warp.cpp`) loads the destination area synchronously and then
writes `sm[0x4c] = 0x80108F60[area]` and returns to the field area machine. For area 1 the machine
runs; for area 2 the next field reaches `Engine::submode1` case 4/5/6, which dispatches the guest
overlay entry `0x80107230` / `0x8010766C` / `0x80107790`, and that dispatch aborts:

```
[watchdog] FAULT (signal): backtrace:
signal = 06
psx::cpu::dispatchGuestWithArgumentsToReturnER4Corej...
Engine::submode1Ev
Engine::stageRunningEv
Engine::frameEv
PcScheduler::runGameStanzaEP4CoreijjiRK5R3000
```

Found while inventorying the load operations for S023: `warp 1` loads and runs, `warp 2` loads and
aborts on the following frame.

**Not a loading defect** and **not caused by the `FUN_80044BD4` owner**: the same abort reproduces
with that override both installed and commented out, and the two runs are otherwise byte-identical.
The load itself completes (`warp: cold area 2 sub 0 loaded at f1590`) before the fault.

The likely cause is that the cold warp loads the MODE/AREA overlay for the destination but the
`sm[0x4c]` entry it selects belongs to an overlay image that is not resident, so the dispatch is an
unsafe fetch. Not diagnosed further here. The proper fix is in the warp/overlay-activation ordering
(`activateModeOverlay` / `retireOverlay` versus the `sm[0x4c]` selection), not in the abort.