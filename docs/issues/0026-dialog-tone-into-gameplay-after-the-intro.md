# 0026 — dialog tone into gameplay after the intro: the Start-skip tone is reproduced, its source is not found

Features: F-AUDIO, F-DEBUG.

Status: open. The warp half and the Start-spam held tone are fixed (below). The natural end of the newgame dialog
is still not driven headless.

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
one of them is current, MusicCoord and `tomba::EngineCd`'s voice play FUN_8001D2A8 (`game/cd/engine_cd.cpp`)
hold back the looping XA area music. So a dialog song that is never stopped also silences the area.

## Fixed: cold warp out of a dialog

To reproduce: `newgame`, `run 1300` (the fisherman dialog, song 4 current), `warp 1`.

Before the fix, the song index stayed 4 for the whole run. Area 1's looping XA request
(`voice_play chan=1 loop=1`) was held back, so the dialog song played on in the new area.

Cause: the old synchronous cold warp (`applyColdWarp`) entered the destination without the song stop
that every guest area transition runs.

Fix: the warp now raises the game's own transition (issue 0034), whose `FieldTransition::main` runs
`FUN_80074E48`. After the warp (song 4 started with `FUN_80074BF8`, `warp 1`) the index is -1 and the area music starts (`coord song=65535 xa_active=1 loop=1`).

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

Warp 5 aborts on HEAD as well (issue 0035). Warp 3 loads since issue 0034.

Because of these, the "warp into an area with a cutscene" route was only checked for area 1.

## User report, 2026-10-27: one frozen note after spamming Start

Spamming Start to get into the game leaks the fisherman cutscene's dialog tone into gameplay, held on
one note. A held note points to an SPU voice left keyed on, not a SEQ still playing. The song-index
comparison above cannot see that. The 200-500 RMS left after the Start skip in the headless WAV may be
that voice. The route is the Start skip itself: check whether a title-owned skip leaves the cutscene
before the guest keys off its voices.

Next step: on the Start-spam route, compare the SPU voice key-on/off and pitch registers per field
against Beetle.

### Fixed, 2026-10-27: the song stop never keyed off its voices

Route: `scratch/player-bugs/start-spam/spam_120.pad` (Start 3 frames on, 5 off, for 120 pad frames from boot), run
unpaced with `PSXPORT_WAV`. Before the fix the mix carried a steady 351 Hz tone from the skip (about 4.5 s) to the end
of the run.

The oracle compare (`tools/oracle_compare.py`, which now compares SPU voices) and a per-frame trace of both cores on
the free-roam Start leg (`scratch/player-bugs-2/keyoff_trace.py`) show it: song 4 keys voice 1 (start `0x1D770`,
pitch `0x721`) on both cores. The console's song keys it off 19 frames later while the area load holds. The product's
load finishes at once, so `FUN_80074E48` stops song 4 four frames after the key-on, and voice 1 stayed keyed for good.

Cause: `Sequencer::channelKeyEventScan` (`game/audio/sequencer_channel_flags.cpp`, guest `0x80095B90`, the voice scan
under `SsSeqStop`) read its voice count from `0x00005CEC`, the cluster offset `kSpuKeyScanCount` used as an address,
instead of `0x80105CEC`. The count read 0, so no stop ever keyed off a sequence's voices.

Fix: read `kBase + kSpuKeyScanCount`. On the same leg the stop now arms the key-off for voices 0-4 and voice 1
releases the next frame. On the Start-spam WAV the 351 Hz share of each second (+-4 Hz) goes from 29/61/20/44/52/42/32%
over 4-10 s to 26/10/0.7/0.1/0.1/4/6%. At the oracle's `free_roam` and `gameplay[0]` checkpoints voice 1 no longer
sounds on the product alone.

The same trace found `AreaSlots::updateTail` (guest `0x80075A80`) calling the key-off `FUN_80098F90(0, mask)`
without loading the mask into `a1`. On this route its one drain keyed off `0x80` (the stale `a1`) instead of the armed
voices `0x7FF00`. It now passes the mask as the guest does.

The earlier note "the warp keys off all 24 voices and the tone stays" came from another product defect: a key latch
written twice in one field keeps only the second value (psxport issue 0183).

### Re-measured on the timed SPU, 2026-10-27
psxport now runs the SPU to the guest's time at every register access (issue 0183). Same route
(`scratch/player-bugs-2/spam-timed`, 1900 fence steps): the 351 Hz share is 24.8/9.4/0.9/0.1/0.1/4.2/5.2% over 4-10 s,
then 0-8.5% to 30 s (8.5% at 11 s, 8.3% at 18 s). The skip second (4 s) still carries a quarter of the energy at
351 Hz; the held tone after it is gone.

Still divergent at the oracle checkpoints (`scratch/player-bugs-2/oracle5.log`): `game_stage` sounds voices 12 and 13
on the product only (peak 4346 against 0); `free_roam` sounds `[0, 1, 2, 4]` against the console's `[0, 1, 2, 3, 6]`;
voice 0's pitch is `0x400` against `0x47D` from `free_roam` on. The `field` checkpoint's SPUCNT CD-enable and CD
volume difference is the guest's own CD-mix sequence at a different point (the product reaches `field` 288 game
frames sooner); they agree again at `gameplay[0]`.

### The remaining oracle divergences are the removed loads, 2026-10-28
Traced per frame on both cores (`scratch/player-bugs-3/song_trace.log`, `game_leg.log`, `gameplay_notes.log`):
`game_stage` voices 12/13 are the menu confirm sound, 17 frames on both cores, which the console plays inside its
37-frame title-to-game load; `free_roam`'s voice sets and voice 0 pitch `0x400`/`0x47D` come from song 4 keying three
more notes during the console's 30-frame area load (sm[0x4e]=8), which the product finishes in one frame; at
`gameplay[1]` those extra releasing voices make libsnd allocate and steal different voices for song 2. The
sequencer, the Start skip and the SPU adapter play the same notes on the same frames otherwise. With silent voices'
leftover registers informational (psxport `tools/oracle/spu_state.py`), `gameplay[0]` matches across cores
(`scratch/player-bugs-3/oracle-b.log`). What stays open here is the natural end of the newgame dialog (above).
