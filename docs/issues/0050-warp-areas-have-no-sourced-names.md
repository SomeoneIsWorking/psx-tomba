# 0050 — Most warp areas have no sourced name

Status: open (2026-11-03). Owner: `game/core/debug/dev_warp.h:DevWarp::kAreaNames`, `game/core/debug/debug_menu.cpp`.

## Observed
`warp ?` lists 20 of the 22 areas as `area N`; only area 8 (Water Temple) and 12 (Ghost Pig boss) carry names, both
from the user (2026-07-23). Entries are `entry N`, a room entry marked `(room R)` from its record flags.

## What the game has
MAIN.EXE holds the map screen's place names ("Kujara Ranch", "Crying Forest", "Water Temple", "Hidden Village", ...).
The map screen's area-to-name table is not traced, so no area is named from it yet. Tracing the map screen's name
lookup gives `kAreaNames` from guest data and retires the hand table.
