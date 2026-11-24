# 0072 — Sequenced songs play at double tempo

Status: open. Owner: `game/resident_natives.cpp` (`Engine::frameUpdate`) with psxport `runtime/psx/frame/timing.cpp`
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
One owner per field: each field's VBlank runs the trampoline exactly once, through the guest's interrupt path when
it is deliverable and directly (acknowledged) when not, as psx-spyro's `FieldOwner::dispatchCallbacks` does.
Dropping the hand call alone under-ticks: the two VBlanks of a logic frame are raised back to back at the present
and the second often latches over the first (the dwell counter 0x800E809C ended at 0 or 1 on every frame).
