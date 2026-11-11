# 0056 — SOP narration in-betweens show the next tick's line place

Features: F-FPS60.

Status: fixed (2026-11-11).

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

## Fix (2026-11-11)
FUN_8010C79C is owned by `game/scene/narration_scroller.cpp:NarrationScroller`, an override real frames and replays
both reach: `step` is the phase machine at sm+0x54 and the scroll and line index (0x8010D394, 0x8010D398), `place`
draws each line from the first still on screen through the same jal sites as the guest (strlen 0x80079528, drawText
0x80079374, bucket 1). In a replay (`ReplayFrame::replaying` gives t) the step still runs, so the scroll it starts from
is the one the last tick drew and the one it reaches is the one this tick draws; the lines keep this tick's words and
`Font::PlacedGlyphs` has `glyphEmit` place each glyph `lerpExact(drawn, stepped, t) - stepped` lines below its word.
At t = 1 the packets are the real frame's.

Differential (`PSXPORT_OVERRIDE_DIFF=0x8010C79C`, `scratch/narration/n6`): 20 of 20 sampled calls match the guest
body byte for byte, 0 incomparable. An earlier version that called `Str::length` directly mismatched every call: the
guest's strlen is a nested override the journal pairs by position, so the native calls it through its jal too.

Shown (`scratch/narration/n7`, boot unskipped into SOP, unpaced, player settings): the narration's top glyph row at
196, 192, 188, 184, 180, 176, 172, 168 sink pixels over consecutive presents, in-betweens (t = 0.5) and real presents
alternating, so each in-between sits halfway between its neighbours (`ny.py`, `narration_halfway.png`).
