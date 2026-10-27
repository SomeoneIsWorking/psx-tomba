# 0026 — dialog tone into gameplay after the intro: the Start-skip tone is reproduced, its source is not found

Features: F-AUDIO, F-DEBUG.

Status: open. The warp half of the report is fixed (below). The Start-spam half is reproduced headless (a held
351 Hz tone); its source is not found. The natural end of the newgame dialog is still not driven headless.

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

### Reproduced headless, 2026-10-27; cause not found

Route: `scratch/player-bugs/start-spam/spam_120.pad` (Start 3 frames on, 5 off, for 120 pad frames from boot, then
nothing), run unpaced with `PSXPORT_WAV`. Start skips the intro text and the fisherman cutscene; gameplay starts near
pad frame 160. From the skip (about 4.5 s into the WAV) to the end of the run the mix carries a steady 351 Hz tone,
under the area music. After `warp 3` it is left alone: a pure 351 Hz tone (peak/mean about 9000) decaying by about
1.5% every 2 s for over 30 s.

What it is not:
- Not the native libsnd sequencer. Its override differential (`nat_seqChannelDispatch` and the bodies it calls, every
  call) mismatches, but a run that continues from the original bodies (`diff2`) has the same tone.
- Not the XA stream. `XaStream::pullSample` returns silence once the dialog clip (chan 7, stopped at LBA 66851 by the
  skip) is inactive, and the tone outlives the switch to area 3's clip.
- Not a voice waiting for a key-off. The warp keys off all 24 voices (`KOFF 18C=FFFF 18E=00FF`) and the tone stays.

Not reverb either. The `spu` debug channel now logs the reverb registers and each voice's ADSR writes
(`psxport runtime/psx/audio/spu_beetle.cpp`). On the same route (`start-spam/rv1`, `rv2`) the reverb output volume
`0x184/0x186` is written four times, all 0, so reverb never reaches the mix; the tone is there from 4 s on all the same.
Every voice's release is exponential at shift 10 or 13 (`ADSR2` `D5EA`, `D3ED`) or linear at shift 0 (`1FC0`), so no
keyed-off voice can ring for 30 s, and `XaStream::pullSample` (the SPU's CD input) returns 0 when inactive. The source
is still unknown; what is left is a voice whose key-off does not take, which needs each voice's ENVX at the warp.

Whether retail does the same is not settled. The console reference (`external/psxport/tools/oracle/CONSOLE.md`)
emulates the SPU, but exposes audio only as a SHA-256 over a hash window's samples; it exports no samples, so it cannot
say whether a 351 Hz tone is present on its route, and it has no per-voice audio comparison. Settling it needs the
console's audio samples (or its voice ENVX) across the same Start taps.
