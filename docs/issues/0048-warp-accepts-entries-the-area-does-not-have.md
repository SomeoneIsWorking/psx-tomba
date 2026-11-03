# 0048: `warp` accepts entries the area does not have

Features: F-DEBUG.

Status: open. Owner: `game/core/debug/dev_warp.cpp` (`DevWarp::arm`).

## Reproduction

Hidden, unpaced, autoskip to free roam (frame ~900), then `warp 0 <entry>` over the control channel, a `pshot` 13 s
after each request.

## Observed vs expected

Entries 1-6 of area 0 land Tomba in the field (`scratch/tomba2/evidence/warp0_entries1to6_small.jpg`). Entry 7 from
free roam, and every later request through 16, is requested and consumed (`[warp] area 0 entry N requested`,
`warp0_entries7to16_log.txt`) and leaves a black picture (`warp0_entries7to16_small.jpg`). Expected: a request for an
entry the area does not have is refused at `arm`, as the area and phase are, and never reaches the field machine.

## What is known

`DevWarp::arm` checks only `entry <= kMaxEntry` (0x3F, the low-byte mask the field machine's case 6 applies). MAIN keeps
one table of 8-byte entry records per area, pointers at `0x800A54A8` (read by `FUN_80078798` for the section name and
`FUN_800782F0` for the visited-section bits, `(record[6] & 0x600) >> 9`). By pointer spacing area 0 has 12 records, so
entries 7-11 are records too: that table does not bound the black entries, and the reader that places Tomba from the
entry byte `0x800BF871` (23 MAIN references, Ghidra `--refs`) is not yet identified.

## Next step

Find the entry consumer among the `0x800BF871` readers (or in the area overlay), take the area's real entry count from
it, and refuse anything past it in `DevWarp::arm`.
