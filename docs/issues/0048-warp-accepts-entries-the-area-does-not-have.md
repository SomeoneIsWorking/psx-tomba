# 0048: `warp` accepts entries the area does not have; entry 7's room picture is unverified

Features: F-DEBUG.

Status: open (2026-11-04). The refusal and entry 7's wait are fixed; whether the tower room's picture (left
of its wall black at 16:9 and 4:3) is the guest's own is not shown. Owners: `game/world/area_entries.h`
(`AreaEntries`), `game/core/debug/dev_warp.cpp` (`DevWarp::arm`), `game/core/sched/synchronous_task_wait.cpp`
(`Scheduler::spawnAndWait`).

## Entry count

Cause: `DevWarp::arm` checked only the 0x3F mask, so `warp 0/12`-`0/16` read area 1's records. An area's
records `{s16 x, y, z; u16 flags}` lie in area order behind the pointer table 0x800A54A8, so its entry count is
the distance to the next area's pointer (area 0 has 12).

Fix: `AreaEntries` owns the table (`record`, `count`); `arm` refuses `entry >= count(area)`.

Shown: `warp 0 12` answers `refused: area 0 has entries 0-11`; `warp 0 9` lands
(`scratch/tomba2b/eref/drive.log`).

## Entry 7 wait

Entry 7 is area 0's only room entry (flag 0x800, room 2). Cause: A00's Tomba init FUN_8010AC20 calls the guest
spawn-and-wait FUN_80044BD4(FUN_8010C740, 0x1C, 0, 3), whose wait yields inside the native GAME stanza; the
yield cannot resume there, so the rest of the init never ran and the area machine waited forever.

Fix: `Scheduler::spawnAndWait` overrides 0x80044BD4: flag 1 returns after the spawn as the guest does; any
other flag runs slot 1 inline to its end in the caller's field (no "Loading....." card, by design).

Shown: `warp 0/7` lands in the tower room and the display flips (`scratch/tomba2c/evidence/0048_entry7_display_samples.txt`).

Not differenced: under `PSXPORT_OVERRIDE_DIFF=0x80044BD4` the original's wait yields inside the differential's
shadow call and `Scheduler::yield`'s longjmp would leave the side-effect journal dangling (it segfaulted in
`SideEffectJournal::admitHostService`). `yield` now aborts with a FATAL line instead
(`scratch/tomba2d/diff/run.log`). An original that waits fields has no native counterpart to difference
against until stage stanzas run on fibers and `yieldJmp_` goes.

## Open: the room picture

Observed: `warp 0/7` shows Tomba on the window ledge, the tower wall from the screen's 40% mark rightwards,
black to its left (`scratch/tomba2d/hud2/grid.jpg`).

Known:
- The warp requests the guest's own area transition (0x800BF839 = 1, destination 0x800BF83A), and the room
  setup on arrival is A00's FUN_8010AC20.
- The room-state draw 0x8003FA44 (called by GAME FUN_80108BE4) has no call to the scenery 0x8003D0BC or the
  tile-grid backdrop 0x8003DF04 that the field draw 0x8003F9A8 makes, so a room draws its model and objects
  only.
- The black edge is the wall's vertical seam, ceiling included, not a ragged cull.
- The HUD ball is not part of the room: after `warp 4/0` it shows at `0/5`, `0/7` and `0/0` alike; before it,
  at none of them (`scratch/tomba2d/hud2/grid.jpg`, `scratch/tomba2d/hud/grid.jpg`).

Not known: a reference. Writing 0x800BF817 = 2 and 0x800BF818 = 1 from `warp 0/5` runs the room controller
FUN_80026AD0 but skips the door object FUN_80073328 (whose case 1 waits on its own swing and 0x800BF816), and
leaves Tomba outside the room, so `scratch/tomba2d/door/strip.jpg` is not a door entry. The door object's own
scripted trigger (0x800BF818 = 5 with 0x800BF817 = its room, its case 0) runs its steps from `warp 0/5`, the
entry straight below the window (x 20224, as entry 7): 0x800BF816-818 go to 1, 2, 2, Tomba gets zone 0x26, and
the room draws left of Tomba, who stands in black right of it (`scratch/tomba2d/door2/strip.jpg`). That trigger
does not check Tomba's position, so this is not a player's door entry either. The console reference
(`tools/oracle/CONSOLE.md`) cannot write RAM, and reaching the tower there needs play.

Next: find the room 2 door object's position in area 0's object pool, stand Tomba there and run the same
trigger, or take a retail capture of the room; then compare.
