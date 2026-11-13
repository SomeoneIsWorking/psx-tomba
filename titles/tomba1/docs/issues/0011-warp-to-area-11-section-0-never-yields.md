# 0011: a warp to area 11 section 0 never finishes a gameplay frame

Status: open (2026-11-13). Owner: unknown (traced only to the gameplay task).

## Reproduction
`scratch/s005t1/run.py` with `survey.py`, `TARGETS="11/0"`, 4:3 (`scratch/t1warp/a11`). Warps to `11/1` and `11/2`
from the same boot arrive normally (`scratch/t1warp/s11_1`, `s11_2`).

## Observed
After `area 11 section 0 entry 0 requested`, gameplay task 0 spends 33 fields without yielding, its budget resumes
spread over collision (`FUN_800443CC` under `FUN_80042C20`/`FUN_80043AB0`) and area-overlay code, then
`[tomba1-frame:error] guest task 0 exited with budget-exhausted`. Sections 0 and 1 share one arrival table
(`0x8007BC70`, entry 0 at (20, -52, 0)), so the arrival position is the same in the working and failing case.

## Next
Dump RAM while task 0 is inside the overrun (`scratch/t1warp/hang.py`, waiting on the first task 0 budget line
after the request) and walk its stack from `sp` near `0x801FE0E8` to the loop that does not yield. Areas 16 (after
14) and 10 then 11 (`0x9484002E` from `0x80060D50`, the libgpu DrawOTag path) are untried since this note.
