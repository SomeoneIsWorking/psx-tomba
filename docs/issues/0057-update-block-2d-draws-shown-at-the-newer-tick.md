# 0057 — 2D draws linked in the update block are shown at the newer tick

Features: F-FPS60.

Status: open (2026-11-11).

## Reproduction
Any gameplay screen with a dialog box, a fade, letterbox bars or a UI sprite in motion, dumped unpaced with player
settings; the 2D draws move on real presents only.

## Observed
Dialog glyphs (`0x8007CC00`), `ScreenFade` (`0x8007E9C8`), `LetterboxBars` (`0x80026864`) and `UiSprite` packets
are identical in an in-between and the following real present.

## Cause
Their writers build and link the packets in the update block, before `game/core/frame/draw_section.cpp:DrawSection`
runs, from state the update steps in the same call. The replay starts from an OT that already holds tick S's
packets and cannot rebuild them at t without running those writers a second time.

## Proper fix
Split each writer's state machine into an update step and a draw the section calls, then track the state the draw
reads (fade level, bar height, glyph reveal, sprite place).
