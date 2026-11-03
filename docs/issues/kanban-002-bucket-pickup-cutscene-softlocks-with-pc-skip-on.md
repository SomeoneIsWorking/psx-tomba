---
id: kanban-002
title: Bucket-pickup cutscene SOFTLOCKS with pc_skip ON (default config)
status: open
labels: [bug, pc-skip]
features: [F-LOAD]
---


USER live 2026-07-15: picking up a bucket initiates a cutscene that SOFTLOCKS under the default config (pc_skip=true). Likely a collapsed-init/wait fork in the cutscene trigger where the pc_skip shortcut branch fails to advance (a wait that never completes, or a phase-gate counter not bumped — cf. the pc_skip frame-counter-bump rule). Repro: replay the bucket-pickup pad capture default config, watch for the cutscene hang after bucket pickup; compare pc_skip=false (SBS Core A / MODE=skip) which should progress. Blocks gameplay progress.

## 2026-11-03: not reproduced on HEAD

The recorded repro is the retired raw pad format: the player refuses it (`[padrec:error] replay REFUSED: ...: not a
phase-keyed pad recording`), so it reaches nothing on HEAD. Replaying its button runs frame by frame from autoskip's
free roam (`pause`, `hold`, `step`) diverges within seconds, because the boot and front end no longer take the frames
they took when it was cut. The next step is a phase-keyed capture: `padrec save <path>` from a player session at the
lock, then reproduce it headless.
`pc_skip` and the flat-setjmp GAME task named above no longer exist; the frame loop is owned by `tomba::FrameDriver`.
The later capture `save-prompt-black-screen.pad`, cut on top of this one, closes the bucket dialog with Circle and
walks on, which says the lock was not on every pickup even then.
