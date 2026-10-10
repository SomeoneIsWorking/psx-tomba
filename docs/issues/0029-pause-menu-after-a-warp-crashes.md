# 0029 — the pause menu after a warp crashes

Status: closed.

## Reproduction

Record path, fps60 off: new game, 200 frames, `warp 8`, 300 frames, `tap start`, 10 to 60 frames.

- Area 8: the process aborted. `host dispatch to 0xDEAD0000 FAILED`; the block began at 0x8011593C.
- Area 1: a budget exit at pc 0x00000000 in `dispatchJalToReturn`.

## Cause

Start was not the pause menu. A warp taken during the scripted opening left the load-mode byte `0x800BF89C` at 2, so
the field ran in the opening's state (sm[0x4e] = 9) where Start skips the intro: `Engine::fieldRun` cases 9, 10, 7,
8, 6 set the target area from `0x800BF83A` (0) and continue in area 0 without reloading the MODE overlay. Area 8's
code stayed in the MODE slot while `FUN_800263E8` seeded the 8-slot array at `0x80100400` with area 0's object
types; slot 2 (type 2) dispatched through `0x8009D314` to `0x801158E0`, which is area 0's tile-grid handler and the
middle of a function in A08. Entered there, `sp` was unadjusted and the saved `ra` slot read 0xDEAD0000 (the
scheduler's top-level return sentinel). Area 1 is the same skip with area 1's code, ending at pc 0.

The dev warp (`game/core/debug/dev_warp.cpp`) did not leave the scripted-sequence mode the way the
skip (case 8) and the attract launch do.

## Fix

`DevWarp::applyArmed` writes load mode 4 before it raises the transition. The load then skips the OPN image and the field starts
in ordinary play, where Start opens the pause menu (Options / Load data / Quit game).

## Verification

`warp 1` and `warp 8`, 300 frames, `tap start`, 60 frames: the pause menu is on screen, no abort, record-path
4:3 recordcheck 689 of 689 frames 0 mismatched each. A hermetic test needs the disc (area load); the evidence is
the scripted run in `scratch/warp-crashes/scripts/f1.txt` and `f8.txt` through `scratch/warp-crashes/drive.py`.
