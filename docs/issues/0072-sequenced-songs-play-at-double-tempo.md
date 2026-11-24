# 0072 — Sequenced songs play at double tempo

Status: fixed (2026-11-25). Owner: `game/resident_natives.cpp` (`Engine::frameUpdate`) with psxport `runtime/psx/frame/timing.cpp`
(`Timing::advanceDisplayFields`).

## Observed
User report 2026-11-20 ("Some tones like dialogue and danger playing at double speed"). The dialogue and danger
tones are libsnd sequences (FUN_80074BF8 songs 4..7, 10, 13); area music is XA and is unaffected.

## Cause
libsnd's VBlank trampoline FUN_800909C0 (user callback, then SsSeqCalled) runs twice per field. `Engine::frameUpdate`
calls it once per field by hand, and psxport raises a VBlank IRQ per field whose guest handler is the same
trampoline, reached through libapi's dispatcher FUN_80085D8C (return address 0x80085E68). Since psxport 155ae6b2
(2026-11-18) an override's original body takes interrupts, so the IRQ path reaches the guest.

Measured from the report's state: song 6 (97 bpm, resolution 384) advances its record's time word (+0x90) by 412 per
logic frame; one tick per field gives 207 (`384 * 97 * 10 / 3600` per field). Tick mode 0x800AC424 is 5 (VSync),
rate 0x80104C2C is 60.

## Fix
`FrameDriver::enterLoop` declares `Timing::titleRunsVBlankHandler()` (psxport d169129e): `Engine::frameUpdate` runs the
trampoline once per field and a field raises no VBlank interrupt for the guest to run it again. Dropping the hand
call instead under-ticks: the guest takes the interrupt only while its own code runs with interrupts open (twice a
frame for frames 3..6 of the report's state, then never), and acknowledging the edges after the present is too late,
since `Timing::advanceDisplayFields` hands the first field's edge to the guest before raising the second.

## Proof
The report's state, 300 logic frames: every frame runs the trampoline exactly twice, all from `Engine::frameUpdate`
(before: four, two of them through FUN_80085D8C), and no `frame-cadence` error: two ticks a frame, 207 for song 6.
