# 0057 — 2D draws linked in the update block are shown at the newer tick

Features: F-FPS60.

Status: fixed (2026-11-11): letterbox bars, the dialog box panel and `UiSprite` calls rebuilt at t; fade fills step on
real presents by design and glyphs reveal whole (below).

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

## Fix (letterbox)
`game/core/frame/update_block_draws.cpp:UpdateBlockDraws` is a motion source. A writer in the update block notes the
packets it linked with the values it built them from, keyed by its entry and the guest object its caller draws for (`render::callSiteOwner`: per call site, the caller
registers holding the object and its loop element); the
section's sample keeps them, and each replay rewrites the later tick's packets from the paired values blended, in
place through its `GuestRedraw` (`writeVertex`, `writeHalf`), so the OT the replay starts from holds them at t.
Unpaired draws are drawn as they are and reported at run end. A note exists only for a writer that ran this tick, and
pairs only while the replay's samples pair (`DrawReplay::endFrame` forgets on a frame with no draw or a cut).

- `LetterboxBars::tick` notes both bars with their height; the replay places them at the unrounded blended edge
  (`lerpExact`), bar height rounded up. Shown halfway on the opening letterbox (`scratch/narration/b3`, again after
  the rebuild moved onto `GuestRedraw` in `scratch/review-tomba/bars`): bar edge 12, 16, 20 ... 96 sink px on
  consecutive presents, in-betweens on the half.
- The letterbox draws only with the effect slots armed (`0x1F80019A == 2`), in gameplay frames the section replays,
  so its state producer (`BarsRender`) was composed only at t = 1, where it equals the guest packets; it is deleted
  and `rebuildBars` is the letterbox's one blend.
- `ScreenFade` is not rebuilt. Every Tomba fade steps one 5-bit unit (8) a tick, and the record raster is 15-bit at
  every ires: psxport `runtime/psx/render/record_rasterizer.cpp` draws into `kVramFormat` (R8G8, one 1555 pixel per
  texel), and `runtime/psx/shaders_gpu/record.frag:pack` takes the command's 8-bit colour `>> 3` before `blend` mixes
  5-bit channels. A halfway fill colour therefore lands on one of the two ticks' levels. Shown on the door fade
  (`door 13 0`, `scratch/map/g5`): the rebuild wrote `0x62F4F4F4` between F8 and F0, and the in-between was
  pixel-identical to the later tick on 3047 of 3528 lit pixels, every lit value on the 5-bit ladder (24, 33, 41,
  49). The rebuild was deleted. An in-between level needs a raster target with more than 5 bits per channel.
  A fade linked after the section is missing from every in-between unless relinked: `Engine::fieldRun` state 10
  (the subtractive fade-out after `fieldFrame`) notes it (`DrawReplay::noteTrailingFade`) and each replay of the next
  interval links it after the section's steps (`DrawSection::draw`), at the older real's level. Shown on the SOP cliff
  fade-out (bug report 2026-11-12 08:27:18, pad replay, `scratch/br2/r3f`): mean picture level per present 119, 119,
  112, 112, 105, 105 ... 9, 9, where the in-betweens had stayed at 125.
- The dialog box: the machine `0x8007D594` steps its open (state 0x5A eases y +0x56 toward +0x82, 0x5B grows the
  width +0x58 by an accelerating step and recentres x +0x54 on 0xA0) and then draws the glyphs (`0x8007CC00`) and the
  panel (`Panel::drawPanel`, `0x8005019C`) from node +0x54 in the same call. Glyphs are not drawn while it opens
  (0x5A..0x5C clear the count `0x1F80017E`) and reveal whole glyphs a tick, so only the panel moves. `Panel::drawPanel`
  notes the packet run with the rect, keyed by the dialog node (s0 at `0x8007DA38`, `0x8007DD88`, `0x8007DEB4`), the
  window record (s1 at `0x8004FD00`) or the save prompt's box (one call a frame); `Panel::rebuild` places the four corner sprites
  and five fills from the rect blended (`lerpExact`), sharing `layoutOf` with `Panel::build`. Shown at `warp 13/0`
  +1035 (`scratch/rr0057/a1`, `panel_rise.png`, `panel_grow.png`): the top edge on consecutive presents is 1672, 1620,
  1568, 1528, 1488 ... 1248, 1240 sink px and the left edge 1608, 1576, 1536, 1496 ... 1160, each in-between on the
  half of its neighbours, corners joined; guest RAM and scratchpad at frame 2405, after the open,
  identical with fps60 off (`scratch/rr0057/o1`).

- `UiSprite` (`0x8007E1B8`/`0x8007E6DC`). Which calls run in the update block, by noting every call there and
  logging the notes (`debug update-draws`) at entry 0 of all 22 areas (`scratch/ui0057/scan1`): only the area label
  node `0x800727D4` -> `0x80072898` (a centred row at y 100 that does not move). The HUD's life and fruit sprites
  (`0x80025744`) are inside the section and replay; the pause menu (`0x800346BC`, `0x8007EAE4`) draws in the page
  step, replayed after the section (0055). The calls that move are the popups whose node behaviour is `0x80072520`
  (spawned by `0x80071B44`, the AP score, and `0x80072114`, a one- or two-sprite popup drawn by `0x80072308`): each tick `0x80071DFC`/`0x80072308`
  projects Tomba's place (or the source node's) through the view and draws the digits there, each bouncing on its own
  phase, through `UiSprite::quads`. `producerEntry` notes each call with its anchor, definition index and table,
  keyed by the popup node (s1 with digit s0 at `0x800720C4`, s3 at `0x800724B4`/`0x800724F8`) or the label node
  (s1 with glyph s0 at `0x80072A2C`); the rebuild moves every piece's vertices by the anchor's blended offset, and a call
  whose definition changed is drawn as it is. Reached with the debug `ap <amount>` (`DevApPopup`, spawns
  `0x80071B44(Tomba, amount, 0)` at a frame boundary). Shown at `warp 0/0` +420, `ap 500`, frame-exact
  (`scratch/ui0057/a1`, `b1` with the note disabled, `popup_rise.png` top a1, bottom b1; `digits.py`): the third
  digit's top edge on consecutive presents is 684, 696, 704, 712, 720, 728, 732, 736 sink px, each in-between on the
  half of its neighbours, where b1 shows 696, 696, 712, 712, 728, 728 (the in-between equal to the newer real).
  Holding right (`r1`, `popup_walk.png`) the digits stay over Tomba's head. Guest RAM and scratchpad at 200 frames
  after the popup identical with fps60 off (`r0`).

## Remaining
- The state producers `Panel`'s list job, `DialogGlyphs` and `UiSprite` register are not composed into a replayed
  interval (`FramePresenter`: a replaying title brings whole in-betweens); they serve presents whose draw ran no
  section, and the dialog box and sprites still need their replay owner above.
