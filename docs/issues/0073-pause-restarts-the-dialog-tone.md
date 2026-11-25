# 0073 — A pause over a dialog tone restarts it

Status: fixed (2026-11-25). Owner: `game/audio/song_switch.cpp` (`SongSwitch`, FUN_80074BF8 / FUN_80074E48).

## Observed
User report 2026-11-20 ("Some tones like dialogue and danger playing at double speed", second half): pausing during
a dialog restarts its music instead of leaving it alone.

## Cause
Opening the pause menu plays sting song 0, closing it song 1. FUN_80074BF8 parks a playing dialog tone (songs 4..7,
10, 13) in 0x800BE22A, stops it with SsSeqStop, and `AreaSlots::updateTail` requests it back once the sting ends;
the request plays it with SsSeqPlay from the start. Retail does the same, but there the area XA music took over
during the pause (the 2026-11-12 report, fixed by `MusicCoord` holding the XA back), so the restart was not heard.

## Fix
`SongSwitch` owns both functions. A tone parked under a sting is paused instead of stopped: libsnd's key-off
0x80091050 and its record's playing bit (+0x98 bit 0, set by the play start 0x80091810) cleared. The parked
request brings it back with the play bit set and 0x80091910 (play mode 1), at the event it stopped on. A parked
tone the game abandons is stopped when another song replaces it.

## Proof
The report's state, start pressed at +20 and +140, song 6's record read every frame (`scratch/music/dump.py`): its
read pointer holds at 0x82D42 and 0x82F4F while stings 0 and 1 play, then goes on from there (before: it ran on
under the sting and was replayed from the sequence start).
