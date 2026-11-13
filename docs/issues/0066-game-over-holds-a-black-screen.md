# 0066 — Tomba! 2 game over holds a black screen

Status: fixed (2026-11-13).

## Reproduction
`spot 1 0 24 8576 -2600 14400` (area 1's death floor, sector 24) from a paused boot stepped to 1050, unpaced,
`ires=0`, `fps60=1`, sink 3840x2160 (`scratch/i0066/drive/go.py`). Any fall that leaves vitality 0 takes the same
route: the death floor, or a water fall at 1 vitality.

## Observed
Vitality reaches 0, the fade closes (`0x800BF80D = 3`), and the field run (GAME `0x80106B98`) sets the task-0
machine (`*0x1F800138`) to running sub-mode `sm[0x4a] = 2` with `sm[0x4c] = 0`. From there nothing changed for
1400 frames and every present was black; `sm[0x4c]` never left 0.

## Cause
`game/core/engine/engine_state_dispatch.cpp:Engine::frame` refused running sub-modes 2..4 as unowned and handed
task 0 to the cooperative guest loop, which the scheduler resumed once and never stepped again. Sub-mode 2 is the
game over screen (GAME `0x80106478`, owned as `Engine::areaLoadState` and already dispatched by
`Engine::stageRunning`), so the refusal was stale and kept its owner from running.

## Fix
`Engine::frame` runs sub-modes 2 and 4 through `stageRunning`. `areaLoadState` runs the guest's leading
`AreaSlots::updateTail` (`0x80075A80`) every frame, as `0x80106478` does. Sub-mode 4, the Quit exit
(`0x801089C4`), is owned as `Engine::leaveGame`: settle the area audio, then `startStage(1)`, the title front end.

Shown (`scratch/i0066/continue`, `scratch/i0066/quit2`): after the fall's fade, "Save?" (Yes / No), then
"Continue / Load data / Quit game". Continue reloads area 1 at entry 1 with full vitality and Tomba in control;
Quit game asks "OK to quit game?" and Yes returns to the title.

Sub-mode 3 (`0x80106A24`) still goes to the cooperative loop, which never resumes task 0 (issue 0067).
