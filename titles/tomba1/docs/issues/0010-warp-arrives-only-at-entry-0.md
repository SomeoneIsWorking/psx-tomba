# 0010: every warp arrival lands on entry 0 of its section

Status: open (2026-11-03). Owner: `titles/tomba1/game/core/debug/debug_menu.cpp:tomba1::DebugMenu`.

## Observed
`warp ?` lists `area N/section S`; each arrival arms `DevWarp::arm(area, section, 0)`. The door at `0x8002CFFC` also
writes an entry (`0x8009C108`), so a section can be entered at more than one place, but the catalogue offers only
entry 0. The old flat `warp <area> [section] [entry]` took any entry 0..255, which named entries the area may not have.

## Next
Trace where state 3 of `areaTransitionHandler` `0x8001CFCC` reads the entry (the per-area entry table and its count),
then list each section's entries from that table, as Tomba 2's `AreaEntries` does (`T1-07a` in `../re-frontier.md`).

## Found (2026-11-13)
`FUN_800243E8` reads the arrival at `0x8007BF78[area][section] + entry * 8`: s16 x, y, z and a flags halfword (bit 0
swaps the axes, high byte the facing). Sections can share one table (area 11: sections 0, 1 and 3 at `0x8007BC70`;
2 and 4 at `0x8007BC80`), and no count is stored beside it. The retail debug select `FUN_8001AD58` keeps the current
entry `0x8009BCEA` rather than choosing one.
