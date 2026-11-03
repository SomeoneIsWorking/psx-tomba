# 0050 — Most warp areas have no sourced name

Status: open (2026-11-03). Owner: `game/core/debug/dev_warp.h:DevWarp::kAreaNames`, `game/core/debug/debug_menu.cpp`.

## Observed
`warp ?` lists 20 of the 22 areas as `area N`; only area 8 (Water Temple) and 12 (Ghost Pig boss) carry names, both
from the user (2026-07-23). Entries are `entry N`, a room entry marked `(room R)` from its record flags. An entry is its
record, so two entries can be one place: Ghost Pig boss entries 0 and 1 are the same record (`0x800A53E0`, x 0xB00,
y -0x11CC, z 0x13B1, flags 0x107) and land on the same picture; Water Temple entries 0, 1 and 2 are three records and
place Tomba at three spots (2026-11-04, `scratch/warp-fix/reduced/t2_strip.jpg`).

## What the game has
MAIN.EXE holds the map screen's place names ("Kujara Ranch", "Crying Forest", "Water Temple", "Hidden Village", ...).
The map screen's area-to-name table is not traced, so no area is named from it yet. Tracing the map screen's name
lookup gives `kAreaNames` from guest data and retires the hand table.
