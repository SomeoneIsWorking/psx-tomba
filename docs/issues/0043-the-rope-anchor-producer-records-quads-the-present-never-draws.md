# 0043 — The rope anchor producer records quads the composed present never draws

Status: fixed (2026-10-27). Owner: `game/render/quad_state.cpp` (`savePackets`, `QuadStateRender`), the state
every guest-body producer listed against `quad_state.h` in `docs/producers.md` shares.

## Observed
Start-spam route (`scratch/player-bugs/start-spam/spam_120.pad`, unpaced, ires 0, sink 3840x2160,
`tools/shipping_settings.ini`): every composed t = 1 present from f707 logged
`[ledger:error] producer 0x8013e9d8 recorded 6 primitive(s) and the composed present drew none`, plus
`0x80122974` (`AnchorQuads::chain`, f665-f699) and `0x80106690` (DEMO edge sprites, f574-f588).

What the player saw (present dumps f570-f700 before and after the fix; crops in `scratch/player-bugs-3/evidence/`):
- f574-f588: the title screen's art was black on every present; only the NewGame/LoadGame labels drew.
- f666-f699: the anchor chain drew only its newest packet per bucket, so its segments sat in the wrong place.
- The rope (0x8013E9D8) was dropped from every composed present; on this route its poly-lines lie above the top
  edge (y -95 to -44), so it is not visible here, but anywhere the camera shows it, it was missing.

## Cause
Two defects in `quad_state.cpp`:
1. `savePackets` walked a bucket from its slot and stopped at the first packet outside the call's
   `[before, after)` span. The span is the pool cursor (24-bit, `0x000DB934`) while every packet after the head
   was read back as a KSEG0 address (`0x800DB95C`), so only the head packet of each bucket was saved; a walk with
   no span ran on through every lower bucket and saved each packet once per bucket above it.
2. `QuadStateRender` decoded each saved packet with `decodePacketPrimitive`, which answers nothing for a
   poly-line or a draw mode packet. The rope's packets are an E1 draw mode and two gouraud poly-lines
   (`0x5E`, 3 segments each), so its render drew none of its 6 recorded lines.

## Fix
The walk compares main-RAM offsets and ends each bucket at the next bucket's slot (`OtTables::slotOf`), skipping
packets outside the span instead of stopping. The render decodes poly-lines through `decodePolyLine` into their
segments and applies the saved draw mode packets (`DrawModeTracker`), as `PacketChainWalker` does.

## Shown
Same route to f1900 after the fix: `[ledger] run-end: 1312 composed t=1 present(s) reconciled, 0 with a dropped
producer` (before: every gameplay present). Before/after crops: `scratch/player-bugs-3/evidence/0043_demo_f580_before_after.png`
(title art restored) and `0043_chain_f690_real_before_after.png`.
