# 0048: `warp` accepts entries the area does not have

Features: F-DEBUG.

Status: bound fixed (2026-11-03); the room entry, area 0 entry 7, is open. Owners: `game/world/area_entries.h`
(`AreaEntries`), `game/core/debug/dev_warp.cpp` (`DevWarp::arm`).

## Reproduction

Hidden, unpaced, autoskip to free roam (frame ~900), then `warp 0 <entry>` over the control channel, a `pshot` 13 s
after each request.

## Observed vs expected

Entries 1-6 of area 0 land Tomba in the field (`scratch/tomba2/evidence/warp0_entries1to6_small.jpg`). Entry 7 from
free roam, and every later request through 16 made after it, left a black picture (`warp0_entries7to16_small.jpg`).
Expected: a request for an entry the area does not have is refused at `arm`, and every real entry lands.

## Cause

Tomba is placed from the entry byte 0x800BF871 by `Pool::setupViewScroll` (FUN_800783DC, `game/world/pool.cpp`): it
reads record `entry` of the area's 8-byte records `{s16 x, y, z; u16 flags}` through the per-area pointer table
0x800A54A8 (also read by `Engine::setAreaStartPos` FUN_80078824 and `SceneTransition::areaMaskTrigger` FUN_800782F0).
The records lie in area order, so an area's entry count is the distance to the next area's pointer, and the last
area's records end at the table itself: area 0 has 12 (0-11), area 1 33, area 2 9. `DevWarp::arm` checked only the
0x3F mask, so entries 12-16 read area 1's records.

Entries 8-11 are real and land when requested from free roam
(`scratch/tomba2b/evidence/0048_area0_entries8to11_small.jpg`); the earlier black pictures for them came from
requesting them after entry 7 had left the field off its running state.

## Fix

`AreaEntries` (`game/world/area_entries.h`) owns the table: `record(area, entry)` for the three readers and
`count(area)`. `DevWarp::arm` refuses `entry >= AreaEntries::count(area)`.

## Shown

`warp 0 12` and `warp 0 16` answer `refused: area 0 has entries 0-11`; `warp 0 9` in the same run lands Tomba in the
field (`scratch/tomba2b/eref/drive.log`).

## Open: entry 7

Entry 7's record (20224, -3775, 5220, flags 0x2B32) is the only area 0 record with flag 0x800, a room entry
(`Pool::setupViewScroll` sets 0x800BF816 and the room id 2 into 0x800BF817). After the warp the field machine moves to
area machine 3 (`sm[4c=3 4e=1]`), the display stops flipping and no in-between is presented again; Tomba's position
is the record's (`[cw]` stores in `scratch/tomba2b/e7b/run.log`, `0048_area0_entry7_small.jpg`). Not traced: what
machine 3 waits for on a room entry that the door into that room supplies and the warp's door request does not.
