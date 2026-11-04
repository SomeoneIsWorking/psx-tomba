# 0048: `warp` accepts entries the area does not have

Features: F-DEBUG.

Status: fixed (2026-11-04): the refusal and entry 7's wait are fixed; entry 7's room picture is the guest's own (below). Owners: `game/world/area_entries.h`
(`AreaEntries`), `game/core/debug/dev_warp.cpp` (`DevWarp::arm`), `game/core/sched/synchronous_task_wait.cpp`
(`Scheduler::spawnAndWait`), `game/core/sched/scheduler.cpp` (`Scheduler::yield`).

## Reproduction

Hidden, unpaced, autoskip to free roam (frame ~900), then `warp 0/<entry>` over the control channel, a `pshot` 13 s
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

`warp 0 12` and `warp 0 16` (the grammar before psxport's warp catalogue) answered `refused: area 0 has entries 0-11`; `warp 0 9` in the same run landed Tomba in the
field (`scratch/tomba2b/eref/drive.log`).

`AreaEntries::count` aborts on a table out of area order; `arm` refuses an area with no entries.

## Entry 7

Entry 7's record (20224, -3775, 5220, flags 0x2B32) is the only area 0 record with flag 0x800, a room entry
(`Pool::setupViewScroll` sets 0x800BF816 and the room id 2 into 0x800BF817).

Cause: on that entry A00's Tomba init FUN_8010AC20 sets 0x1F80019C to 3, puts the area machine in state 3
(`sm[4c=3 4e=1]`) and calls the guest FUN_80044BD4(FUN_8010C740, 0x1C, 0, 3): spawn the room task in slot 1 and wait
for its done flag 0x1F80019B. The guest wait yields through FUN_80051F80 inside the native GAME stanza, which cannot
resume a guest continuation; `Scheduler::runGameStanza` catches the yield and the rest of FUN_8010AC20 (clearing
0x1F80019C, starting the area through 0x80074F24) never runs. State 3 waits on that flag forever: the OT stays empty
and the display stops flipping. `Scheduler::spawnAndWait` already held the native body but had no caller.

Fix: `Scheduler::spawnAndWait` is the override at 0x80044BD4. A flag other than 1 runs slot 1 inline
(`runSyncSlot`) until it ends and aborts if the done flag is still clear. Flag 1 returns after the spawn, as the guest
does (`beq $s3,1` at 0x80044C54).

Where it differs from the guest, by design (load removal): the guest waits one field per loop pass, and for flag 2
(GAME.BIN site 0x8010270C) each pass also bumps the frame counter 0x1F800198 and draws the "Loading....." card
(FUN_8007FD54, which blinks on bit 2 of that counter). The native pumps the task to its end inside the caller's
field with its countdowns zeroed, so no field passes, the counter is not bumped and no card is drawn.

Not differenced: an original that waits fields cannot be differenced against a native that waits none; the
comparison is the disassembly of 0x80044BD4-0x80044CD0 against the body above. Under
`PSXPORT_OVERRIDE_DIFF=0x80044BD4` the original's wait yields inside `OverrideDifferential::shadow`, and
`Scheduler::yield` (`game/core/sched/scheduler.cpp`) longjmps out of the GAME stanza past the open
`SideEffectJournal::Scope`, leaving `Core::sideEffectJournal` pointing at a dead stack journal; the next host service
dispatched through it faulted (signal 11 in `SideEffectJournal::admitHostService`,
`scratch/tomba2c/evidence/0048_spawnandwait_diff_fault.txt`, `scratch/tomba2d/diff_before/run.log`). `yield` now
aborts with `FATAL: yield ... would longjmp out of an open side-effect journal` before the jump
(`scratch/tomba2d/diff/run.log`). The proper fix is stage stanzas on fibers, so a yield returns through every host
frame; until then a differential of a waiting function stops at once and says why.

Shown: `warp 0/7` from free roam lands Tomba inside the tower room, 0x1F80019C 00, 702 polygons; `warp 0/8` to
`warp 0/11` requested after it in the same run all land
(`scratch/tomba2c/evidence/0048_area0_entries7to11_seq_small.jpg`, `scratch/tomba2c/e8/out/drive.log`). The runs
from boot to free roam, `warp 1/0` and `warp 14/0` log no FATAL and no caught GAME yield. The display flips across
frames: twelve samples 40 frames apart over 480 frames after the arrival show both buffers, 702-715 polygons, and
`disp` on successive sampled frames 400 frames in alternates (0,0) and (0,256)
(`scratch/tomba2c/evidence/0048_entry7_display_samples.txt`).

The room picture is the guest's own. The door route (A00's door object FUN_80073328 sets 0x800BF818 to 1; the room
controller FUN_80026AD0 sets `sm[4c=3 4e=0]` and 818 to 2; `fieldRunX` state 0 runs area 0's enter hook FUN_8010CB60
from the table at 0x800A4AF8) was run without steering by writing 817=2, 818=1 from free roam. It reaches the same
room machine and the same picture as entry 7: `rect=0 env=0`, no HUD ball, black beyond the room model
(`scratch/tomba2d/door/strip.jpg`). Machine 3's draw twin 0x8003FA44 has no call to 0x8003D0BC (overlay scenery) or
0x8003DF04 (the tile-grid backdrop), so a room draws only its model and objects over black. The hook's other work is
Tomba's placement: zone byte 0x800E7EAA (G+0x2A) = 0x26, 0x800E800E = 0, a half turn when 0x800BF8BC is -1, the yaw
into 0x1F800210, then FUN_8010DA70, whose body equals entry 7's FUN_8010C740. With zone 0x26 Tomba stands on the
tower floor (`scratch/tomba2d/poke/strip.jpg`); entry 7's record flags give zone 0x32, the window ledge. 0x1F800210
turns Tomba only; the room camera is fixed (`scratch/tomba2d/yaw/grid.jpg`). The room is a cutaway that the guest
draws over black at both aspects; no retail capture of this room is in the repo to compare its framing against.
