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
returns 0, and `game/core/sched/guest_task_stanzas.cpp:Scheduler::runDispatchStanza` re-enters the guest loop top
from it; after that first entry task 0 makes no further step. Owning sub-mode 3 natively in `Engine::frame` would
leave the hand-back unreachable and deletable; its one guest callee not yet located is the overlay call
`0x80112660(0/1)`.
