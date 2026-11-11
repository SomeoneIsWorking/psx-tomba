# 0057 — 2D draws linked in the update block are shown at the newer tick

Features: F-FPS60.

Status: partly fixed (2026-11-11): letterbox bars and fade fills rebuilt at t; dialog glyphs and `UiSprite` open.

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

## Fix (letterbox, fade)
`game/core/frame/update_block_draws.cpp:UpdateBlockDraws` is a motion source. A writer in the update block notes the
packets it linked with the values it built them from, keyed by its entry and its slot or call-site object; the
section's sample keeps them, and each replay rewrites the later tick's packets from the paired values blended, in
place, so the OT the replay starts from holds them at t. Unpaired draws are drawn as they are and reported at run end.

- `LetterboxBars::tick` notes both bars with their height; the replay places them at the unrounded blended edge
  (`lerpExact`), bar height rounded up. Shown halfway on the opening letterbox (`scratch/narration/b3`): bar edge
  12, 16, 20 ... 96 sink px on consecutive presents, in-betweens on the half.
- `ScreenFade::drawEntry` notes its fill with its colour and blend mode; the replay writes `lerpColor` of the two
  fill words when the blend matches. Every Tomba fade steps one 5-bit unit (8) a tick, and the framebuffer is
  15-bit, so the halfway colour truncates to the later tick's and a fade's in-between cannot differ in the picture.

## Remaining
- The dialog box: the machine `0x8007D594` steps its open (state 0x5A eases y +0x56 toward +0x82, 0x5B grows the
  width +0x58 and recentres x +0x54) and then draws the glyphs (`0x8007CC00`) and the panel (`Panel::drawPanel`,
  `0x8005019C`) from node +0x54 in the same call. Not started.
- `UiSprite` (`0x8007E1B8`/`0x8007E6DC`): 17 call sites, not started.
- The state producers these writers register (`FadeRender`, the letterbox's, `Panel`'s list job, `DialogGlyphs`,
  `UiSprite`'s) are not composed into a replayed interval (`FramePresenter`: a replaying title brings whole
  in-betweens), so they blend nothing while Tomba replays; whether any present still composes them decides
  whether they are dead code.
