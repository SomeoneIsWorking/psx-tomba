# 0066 — Tomba! 2 game over holds a black screen

Status: open (2026-11-13).

## Reproduction
`spot 1 0 24 8576 -2600 14400` (area 1's death floor, sector 24) from a paused boot stepped to 1050, unpaced,
`ires=0`, `fps60=1`, sink 3840x2160 (`scratch/m0062/drive/p4.py`, runs `pit0c`, `pitv2`). The same route is taken
by any fall that leaves vitality 0: the death floor, or a water fall at 1 vitality.

## Observed
Vitality reaches 0, the fade closes (`0x800BF80D = 3`), and the field run steps the scene machine
(`*0x1F800138`) to `+0x4A = 2, +0x4C = 0`. From there nothing changes for 1400 frames: Tomba's state, the scene
machine and the fade bytes hold, and every present is black. Retail shows the game-over screen.

## Cause
Not traced. `Engine::frame` (`game/core/engine/engine_state_dispatch.cpp`) does not own running sub-mode 2 and
hands it back to the cooperative guest loop; whether that handler waits on something the port never supplies is
the next question.
