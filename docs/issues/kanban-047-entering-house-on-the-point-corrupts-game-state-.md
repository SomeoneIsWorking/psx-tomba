---
id: kanban-047
title: pc_skip: entering 'House on the Point' corrupts state (music stops, camera unfollows, interior vibrates)
status: open
labels: [bug, pc-skip]
---


USER 2026-07-23, reported from their LIVE windowed game. Entering the 'House on the Point' interior corrupts the game state. NOT a freeze or crash — the game keeps running (frame advances ~30fps, no historical guest-entry miss, no abort). The corruption is BEHAVIOURAL / state, multiple symptoms at once:
  - the BGM/music STOPS on entry;
  - the house INTERIOR GEOMETRY VIBRATES (per-frame jitter);
  - on going back OUTSIDE, the CAMERA STOPS FOLLOWING Tomba.
This cluster (music + camera + geometry all wrong together) points at a single upstream cause — a corrupted scene/area-load or a state block clobbered on the interior transition — rather than three independent render bugs. RE the House-on-the-Point interior LOAD / scene-transition path and find what state it corrupts.

REPRO (cut from the user's live session with padrec): replays/bugs/house-on-the-point.pad, 4134 frames from boot, hand-played route entering the house. Replay headless: PSXPORT_PAD_REPLAY=replays/bugs/house-on-the-point.pad ./scratch/bin/tomba2_port (see replays/README.md).

VERIFICATION METHOD — this is a STATE bug, do NOT crash-check: a headless replay exits 0 while the state is wrong. Verify by observing the actual symptoms: (a) camera-follow — probe the camera-target/follow latch and whether it tracks Tomba's world pos after exit; (b) music — the BGM sequencer state (0x801054B0 open-seq / 0x80104C28 playmask, PSXPORT_DEBUG=seq) going silent on entry; (c) interior vibration — capture consecutive frames of the interior and diff for per-vertex jitter. Compare against a CORRECT house entry if one can be reached, or against the oracle (PSXPORT_GATE=1 + the same replay) to see whether the corruption is exec-state (present on the oracle too = a real state bug) or pc_render-only.
FIRST TRIAGE STEP: run the replay under PSXPORT_GATE=1 (retired comparison path, the oracle) and see if the symptoms reproduce there. If yes, it is a genuine game-state corruption in the transition logic (highest priority, a pc_faithful/Job-#1-class bug); if the symptoms are pc_render-only, it is a render/producer issue.
