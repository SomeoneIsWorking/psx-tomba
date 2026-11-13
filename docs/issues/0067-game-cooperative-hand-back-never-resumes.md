# 0067 — GAME's cooperative hand-back never resumes task 0

Status: open (2026-11-13).

## Reproduction
Before 0066's fix: `spot 1 0 24 8576 -2600 14400` with `PSXPORT_DEBUG=sched,stage,gframe`
(`scratch/i0066/go1/run.log`). Today only running sub-mode 3 (GAME `0x80106A24`) takes this path.

## Observed
`[gframe] ret0 s48=2 s4a=2 unowned-submode`, `[sched] GAME -> cooperative guest loop`, then one
`[sched] slot 0 st_in=2 resume_pc=0x801063F4` and no further step of task 0: `sm[0x4c]` stayed 0, so the guest
handler's first state never ran.

## Cause
Not traced. `game/core/sched/scheduler.cpp:Scheduler::runGameStanza` sets `gameCoop_[slot]` when `Engine::frame`
returns 0, and nothing but the save state reads it.
