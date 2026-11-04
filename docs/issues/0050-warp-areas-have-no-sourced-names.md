# 0050 — Most warp areas have no sourced name

Status: fixed (2026-11-04). Owner: `game/world/area_entries.h:AreaEntries` (`regionName`, `placeName`),
`game/core/debug/debug_menu.cpp`.

## Observed
`warp ?` listed 20 of the 22 areas as `area N` from a hand table, `DevWarp::kAreaNames`; only area 8 (Water Temple)
and 12 (Ghost Pig boss) carried names. An entry is its record, so two entries can be one place: Ghost Pig boss
entries 0 and 1 are the same record (`0x800A53E0`) and land on the same picture.

## Cause
The map screen's names were not traced, so the menu had no guest source for them.

## What the game has
The map screen names an area and an entry with two MAIN.EXE lookups:
- FUN_800787F8(area): the region, `*(0x800A2B64 + 4 * u8[0x800A55C8 + area])`.
- FUN_80078798(area, entry): the place, `*(0x800A2B84 + 4 * (u8[0x800A55B0 + area] + ((flags & 0x600) >> 9)))`,
  with `flags` the entry record's flag word. The same place index picks the visited bit in 0x800BFE50
  (`SceneTransition::areaMaskTrigger`).

## Fix
`AreaEntries::regionName` and `placeName` read those tables; `placeIndex` is the one place-index rule, shared with
`areaMaskTrigger`. The Debug tab names an area `region: place of entry 0` and an entry by its place, with `(room R)`
for a room entry; the hand table is deleted. The guest strings break lines with `\n`, which the menu shows as a space.

## Shown
`warp ?` lists `0 The Starting Beach: Town of the Fishermen`, `8 Water Temple: Water Temple`,
`12 Evil Pig Battle: Evil Ghost Pig`, and area 0 entry 7 as `Waterfall of the Heavens (room 2)`; every area from 0
to 21 has a name (`scratch/tomba2d/walk/drive.log`). Areas 2 and 3 share `Large Mine Underground: Coal-Mining Town`,
as the map screen does; routes stay `warp <area>/<entry>` by number.
