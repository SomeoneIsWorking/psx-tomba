# 0056 — SOP narration in-betweens show the next tick's line place

Features: F-FPS60.

Status: open (2026-11-11).

## Reproduction
New game from the title, into SOP while the narration scrolls (sm[0x52] == 1); dump consecutive presents unpaced,
`ires=0`, `fps60=1`, `PSXPORT_PRESENT_SINK=3840x2160` (`scratch/review-fix/sop_dump.py`, run `soprun`).

## Observed
The narration's top glyph row per present, sink pixels: 504, 504, 496, 496, 488, ... Each in-between sits at the
following real present's place instead of halfway, so the text moves on real presents only.

## Cause
`game/core/frame/draw_section.cpp:DrawSection` runs the text scroller FUN_8010C79C inside the SOP replay, and the
scroller steps its own scroll (`DAT_8010d394`, s32, -1 a frame; line index `DAT_8010d398`) before it places the
lines, so every replay draws the lines one step on, whatever t is.

## Proper fix
A native scroller that splits FUN_8010C79C into its step (update) and its line placement (draw), with the scroll
word as a track and the glyphs placed at the unrounded in-between through `Font::drawText`/the glyph emitter.
A whole-unit Linear track over the guest scroller cannot work: the scroller's own step lands on the blended value,
so in-betweens would alternate between the two ends.
