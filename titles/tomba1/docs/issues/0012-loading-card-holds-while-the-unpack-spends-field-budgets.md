# 0012: the loading card holds while the unpack spends whole field budgets

Status: open (2026-11-14). Owner: `titles/tomba1/game/core/task/disc_loader.cpp:tomba1::DiscLoader`. Features: S024.

## Reproduction
`scratch/s024/plans/warp.py` with `TARGETS=1/0` (through `scratch/s005t1/run.py`, `DRIVE_EXE=scratch/bin/tomba1/SCUS_942.36`),
unpaced and hidden: run `scratch/s024/w2`.

## Observed vs expected
After the area change's own steps, 34 frames show the blinking NOW LOADING card (`0x8001CA84`) before area 1 runs
(`scratch/s024/w2_sheet.png`); New Game shows it for 43. Expected: the load ends in the field that requests it, as the
drive waits now do.

## Found
The loader `0x80021340`'s unpack step (+0x48 = 5) runs the LZ decoder `0x8003EF50` and `0x8005B78C`, and each of
those frames is a budget exit of slot 2 (`guest task 2 used its whole field budget`, 34 of them over `warp 1/0`). Letting
`DiscLoader` resume the slot past its budget in the same field ran the whole load in one field, and then the frame's
`DrawSync(0)` (`0x8005EB54` → `0x80061338`, the queue drain at `0x80061394`) used up its own budget in
`finishMainIteration` and aborted (`scratch/s024/w1/run.log`), so that change was not kept.

## Next
Trace why the libgpu queue drain spins after a field that ran more than one budget of guest work: which of GPUSTAT
bit 26 or the DMA2 busy bit stays set, and which device step clears it on the field edge.
