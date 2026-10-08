# 0026 — dialog music into gameplay after the newgame intro dialog: not reproduced headless

Status: open. The warp half of the report is fixed (below); the newgame half has no reproduction.

## What the guest does

`0x800BED80` is the libsnd current-song index, not a "dialog is up" flag.

- `FUN_80074BF8(song)` stops the previous song (`FUN_80074E48`), sets the index and starts the song
  with `SsSeqPlay` (`FUN_80090560`).
- `FUN_80074E48` runs `SsSeqStop` (`FUN_80091AF0`) on the current song and sets the index to -1.
- Songs 4..7 are reached through `Sfx::trigger` ids 114..117 (the jump table at `0x80016C04`).
- A dialog ends in `FUN_80042310`, which runs sfx id 127 → `FUN_80074EEC` → `FUN_80074E48` when
  the index is above 3. It then restarts the area's XA music through `FUN_80074F24(area)`.
- Every area transition stops the song in `FieldTransition::main` state 4 (`FUN_80074E48`).

`MusicCoord::dialogToneActive` (`game/audio/music_coord.cpp`) reads songs 4..7 as "dialog up". While
one of them is current, MusicCoord and psxport's `voice_play` (`runtime/psx/cd/cd_override.cpp`)
hold back the looping XA area music. So a dialog song that is never stopped also silences the area.

## Fixed: cold warp out of a dialog

To reproduce: `newgame`, `run 1300` (the fisherman dialog, song 4 current), `warp 1`.

Before the fix, the song index stayed 4 for the whole run. Area 1's looping XA request
(`voice_play chan=1 loop=1`) was held back, so the dialog song played on in the new area.

Cause: `game/core/debug/dev_warp.cpp:applyColdWarp` entered the destination without the song stop
that every guest area transition runs.

Fix: `applyColdWarp` calls `FUN_80074E48` first. After the fix the index is -1 after the warp and the
area music starts (`coord song=65535 xa_active=1 loop=1`).

## Not reproduced: newgame intro dialog → gameplay

These routes were compared against Beetle (`scratch/bugs/tools/twocore_audio.py`,
`console_audio.py`), reading the song index, the resume slot `0x800BE22A` and the song's libsnd
status byte:

| route | console | product |
|---|---|---|
| no input, 7000 frames from GAME | -1 for 1380, then 4 for 1624, then 6 | same sequence and durations (CD loads removed) |
| Cross through the intro, Start through the fisherman cutscene, 900 frames of free roam | -1, then song 2 for 38 frames, then -1 | -1, then song 2 for 39 frames, then -1 |

The headless WAV (`wav` REPL command) across the Start skip drops from about 4000-8000 RMS to about
200-500 within 6 frames of the stop. The dialog SEQ does go silent.

Neither core finishes the fisherman cutscene's dialog with Cross, Circle, Square or Triangle taps; the
console ran 20,000 frames. So the natural end of that dialog, which is the user's route, has not been
driven headless.

Next step: record the user's route as a pad replay through the dialog's natural end
(`dbgclient.py padrec save`). Compare the song index and the `FUN_80042310` calls against the console
at the frame where the dialog closes.

Warps 3 and 5 abort on HEAD as well, so this is pre-existing:

- 3: `transitionAreaEnter` faults at `0x801127F8`.
- 5: `dispatchObj` exhausts its budget at `0x8013500C`.
- 2: kanban-122.

Because of these, the "warp into an area with a cutscene" route was only checked for area 1.
