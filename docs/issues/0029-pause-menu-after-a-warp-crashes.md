# 0029 — the pause menu after a warp crashes

Status: open.

## Reproduction

Record path, 4:3 or 16:9, fps60 off: `newgame`, `run 200`, `warp 8`, `run 300`, `tap start`, `run 10`.

- Area 8: the process aborts. `host dispatch to 0xDEAD0000 FAILED`; the block that produced the target
  began at 0x8011593C (`scratch/logs/gate-menu8ref-20261007-151109.log`).
- Area 1 (`warp 1`): a budget exit resumes at pc 0x00000000 and the process aborts
  (`scratch/logs/gate-menu1-20261007-151143.log`).

The area 8 crash reproduces on a build of clean HEAD 196e370, so it predates the fade and emitter work.

## Cause

Not traced. Untested hypothesis: 0xDEAD0000 is a sentinel return address reaching a `jr`, so the warp
leaves some call state the menu relies on unset.

## Blocks

The 16:9 check of the menu backdrop `FUN_80034548` (0027).
