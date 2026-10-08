// engine.cpp — PC-native ownership of the GAME stage state machine (the
// per-area scene/update driver), the engine's top-level "run the game"
// sequencer. Boundary: the ENGINE owns the scene-state machine (which state
// runs, what fields reset on each transition); the actual per-state system work
// (asset load, fade, render, gameplay sub-machines) stays dispatched to the
// retained PSX content/system code. Realizes "the engine runs the game; PSX
// only drives content" for the stage driver — incrementally.
//
// The GAME stage is overlay \BIN\GAME.BIN (LBA 1882), loaded RAW to base
// 0x80106228; task-0 entry = 0x8010637C, which runs as a COOPERATIVE TASK (it
// loops forever, yielding once per frame via FUN_80051f80). The current task
// object ptr is *0x1f800138 (== task-0 obj 0x801fe000); its state fields are
// read through `TaskSm` in game/core/task_sm.h, which is the one place that
// record's layout is written down.
// Three-level nested machine (all handlers in GAME.BIN; full RE in
// docs/engine_re.md):
//   sm[0x48]: 0 -> 0x801086e0 (area INIT)   1 -> 0x80108720 (RESUME-INIT)   2
//   -> 0x80108784 (RUNNING) sm[0x4a] (in 0x80108784, 6-way @0x8010631c): 0..5
//   -> sub-handler (mode 2 = the 9-state sm[0x4c] area
//            load/intro/play machine at 0x80106478, jump table @0x8010622c)
//
// OWNED HERE: all three sm[0x48] handlers. The area-INIT pair (==0 / ==1) are
// clean `jr ra` functions whose callees are SYNCHRONOUS, so a native override
// that mirrors their guest writes and dispatches their resident setup fns is
// faithful (verified RAM 0-diff @ a field frame, later-168). The RUNNING
// dispatcher
// (==2, 0x80108784) owns the 6-way running-sub-mode (sm[0x4a]) selection, but
// its sub-handlers YIELD DEEP (they call resident 0x8007xxxx fns that wait
// across frames). It can NOT be address-dispatched (that nests a test-only reference execution with
// its own CORO_SENTINEL; the deep yield's longjmp destroys that C frame and the
// resume mis-reads the return as task-end, killing task 0 — later-168). It uses
// the cooperative-yield handshake (requestGuestContinuation, later-169): native
// dispatch, then hand to the handler IN-CONTEXT so the yield is the scheduler
// returning, not a nested sentinel. Same mechanism unlocks owning
// FUN_80052078/FUN_800499e8.
//
// WHERE THE BODIES ARE. This file used to hold all 47 of the class's methods, 3,251 lines — the
// largest single file in the repository and one the structure gate had to carry a shrink-only cap
// for. The class is unchanged; only the translation units moved, and they were split by the level of
// the guest's own state machine the bodies serve, so a reader looking for one level has one file:
//
//   game/core/engine.cpp               — this banner, the includes, and nothing else.
//   game/core/engine_task_machine.cpp  — sm[0x48]'s top level: the area-init and resume-init pair, the
//                                        sm[0x4c] area load/intro/play machine, the resident 0x810C page
//                                        submits, the running state and its sub-mode selector, and the
//                                        scene-event FIFO.
//   game/core/engine_state_dispatch.cpp— the one `frame()` step, the stage prologue/body/main trio, and
//                                        the area, scene-state and per-frame mode dispatchers.
//   game/core/engine_field_run.cpp     — the field run and frame-X families, native and guest-faithful.
//   game/core/engine_scene_frame.cpp   — the scene render-list builder and the dev-teleport apply.
//   game/core/engine_object_leaves.cpp — the small per-object behavior leaves and the GPU-state mutator.
//   game/core/engine_frame_ticks.cpp   — the frame-boundary work and the stage bootstrap.
//
// Every one of those is the SAME class, `Engine`, and game/core/engine.h remains the single
// declaration of every method. Nothing was moved to a different class or subsystem, and no method
// body changed: this is a change of file, not of behaviour.
