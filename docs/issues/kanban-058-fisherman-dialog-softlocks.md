---
id: kanban-058
title: Fisherman dialog SOFTLOCKS
status: open
labels: [bug]
evidence: docs/reference/issues/issue58_fisherman_dialog_softlock.png
---


USER 2026-07-23 with a capture: talking to the FISHERMAN (seaside, by the fishing spot with the round saw-blade prop) SOFTLOCKED the dialog. The game does not crash; the dialog simply never advances and control is never returned.

THIS IS THE THIRD DIALOG-PATH SOFTLOCK, and the family matters more than the instance: #2 (bucket-pickup cutscene softlock, pc_skip ON) and #5 (save-sign inspect softlock, pc_skip ON) are the same shape — a dialog/interaction sequence that parks forever. #5 is CLOSED and #2 was root-caused to a delay-slot constant in beh_prng_velocity_machine, then re-parked one layer deeper on script op 0x01 waiting on the unowned FUN_8007DA50. Read those two cards BEFORE investigating: `python3 tools/findings.py softlock dialog` first.

STRONG PRIOR — CHECK THIS BEFORE ANYTHING ELSE: kanban #50 established that under pc_skip the GAME task is a FLAT setjmp task, so any cooperative FUN_80044BD4 wait reached from inside the pc_skip GAME frame gets its C++ stack DISCARDED by the longjmp — everything after the wait silently never runs. That is exactly the shape of "a sequence parks forever and never advances". #50's general fix (PcScheduler::spawnAndWait running the body inline) landed 2026-07-23; if this capture predates that build, RE-TEST ON CURRENT MAIN FIRST — it may already be fixed. If it still softlocks on current main, #50's fix did not cover this path and that is itself the finding (note #50 deliberately did NOT delete four `native_area_load_bd4` flag==1 fire-and-forget sites, whose spawned task still runs flat — a known remaining half of the same disease).

DIAGNOSIS METHOD (state, not crash — the process keeps running):
  - `PSXPORT_DEBUG=sched` — does "caught a GAME substate yield" fire at the moment of the lock? That is the #50 truncation signature.
  - Watch the dialog state machine: DialogBoxSm (game/ui/dialog_box_sm.cpp, guest FUN_8007D594) and the script interpreter (ScriptInterp). #2's investigation used `DialogBoxSm::step` counts and script-op tracing to see WHERE it parks — reuse that.
  - REPL `r`/`rw` on the dialog/script state words, and `watch <addr>` + PSXPORT_CW_BT=1 for a store backtrace on whatever should advance and does not.
  - Compare against the faithful branch if the route is reachable there (mPcSkip=false) — if it only locks under pc_skip, it is the fork/scheduler class again.

REPRO: no replay exists for the fisherman yet. The fisherman is near the seaside fishing spot. CUT ONE FROM THE USER'S LIVE SESSION — `python3 external/psxport/tools/dbgclient.py padrec save replays/bugs/fisherman-dialog-softlock.pad` while they are at/near the lock (the whole session from boot is kept in memory, so it can be cut after the fact), then add a replays/README.md entry in the same commit. That is how house-on-the-point.pad was captured.
