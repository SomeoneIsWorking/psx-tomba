# 0067 — GAME's cooperative hand-back never resumes task 0

Status: fixed (2026-11-13).

## Reproduction
Before 0066's fix: `spot 1 0 24 8576 -2600 14400` with `PSXPORT_DEBUG=sched,stage,gframe`
(`scratch/i0066/go1/run.log`). Today only running sub-mode 3 (GAME `0x80106A24`) takes this path.

## Observed
`[gframe] ret0 s48=2 s4a=2 unowned-submode`, `[sched] GAME -> cooperative guest loop`, then one
`[sched] slot 0 st_in=2 resume_pc=0x801063F4` and no further step of task 0: `sm[0x4c]` stayed 0, so the guest
handler's first state never ran.

## Cause
`game/core/engine/engine_state_dispatch.cpp:Engine::frame` returned 0 for sub-mode 3 (GAME `0x80106A24`, the
ending), and `Scheduler::runGameStanza` then handed task 0 to the cooperative guest loop, which
`Scheduler::runDispatchStanza` entered once and never stepped again.

## Fix
Sub-mode 3 is `tomba::Ending` (`game/scene/ending.cpp`): END.STR, then LOGO.STR, then the Quit exit (sub-mode 4).
A0L's player `0x80112660` is `tomba::StrMovie`, shared with DEMO's opening movie. `Engine::frame` now owns every
state and fails fast on one the guest has no handler for; the hand-back, `gameCoop_`, `Engine::stageMain` and the
loop-top resume are gone (`tomba2.cd` v4).

## Proof
`warp 1/1`, then the `ending` debug verb (the field's own pending transition 8), `scratch/m0067/end1`: sub-mode 3,
END.STR plays its 1200 frames, LOGO.STR its 255, sub-mode 4 leaves to the title, OP.STR and the attract demo follow.
