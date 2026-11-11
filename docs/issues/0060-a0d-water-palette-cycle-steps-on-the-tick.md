# 0060 — A0D's water colour cycle steps on the tick at 60 fps

Features: F-FPS60.

Status: fixed (2026-11-12).

## Reproduction
`warp 13/0` from a paused boot stepped to 1050, step 1800 (the boss intro with the view still), unpaced, `ires=0`,
`fps60=1`, sink 3840x2160 (`scratch/wave0058/wave_plan.py`, run `h1`). The water left of the bridge
(sink x 0..480, y 1100..1700).

## Observed
The water's mean change between consecutive presents is 3.9, 1.2, 1.1, 8.4, 2.3, 0, 0, 8.1, 0, 2.5, 2.4: the small
halfway steps are the screen wave (fixed in 6c11339), and every other tick a large change lands whole on one
present; on in-between 8 every one of the 216 moving 32 px tiles equals the following real (`whole.py`,
`greens.py`, `h1_whole.png`).

## Cause
A palette cycle, not geometry: VRAM row (816, 250), the water's CLUT, steps one frame every other tick. Its owner is
A0D FUN_8010B288, run from the object update FUN_8010B3A8: one descriptor at 0x801175E0 {script 0x80117510, frames
0x80117520, x 816, y 250}, countdown byte 0x801258A8; when the countdown runs out the script's {frame, ticks} pair
uploads `frames + frame * 32` to the row with LoadImage (0x8010B364 → MAIN FUN_80081218), and {0xFF, 12} rewinds it:
frames 0..5, two ticks each. An in-between is spliced onto the newer record and
`psxport runtime/psx/present/record_cluts.cpp:RecordCluts::resolve` gave its primitives that record's entries, so the
step showed on the in-between before the tick it belongs to.

## Fix
The cycle is declared, not inferred (`game/render/palette_cycles.cpp`, psxport `palette_cycle.h`): the same scripted
routine is compiled into each overlay that cycles palettes, and `PaletteCycles::declare` reads each resident one's
descriptors (row, frame table, the frames its script reaches). A declared row both ticks sampled as two different
frames of its table blends from the earlier frame to the later at t = k/n (`RecordCluts::resolve`); any other CLUT
change still lands on its tick. The generic blend of every changed CLUT (psxport 7a9937b9) is removed. Owners
declared: A0D FUN_8010B288; A00 FUN_80115B88 (12 rows at x 1008, descriptors 0x80147D84, countdowns 0x8014C4F0) and
FUN_8013C0BC (row (480, 106), 0x8014ABDC); SOP's grid draw tail (3, 0x8010D2FC, `TileGridLayer::cycleSopPalettes`);
and A00's object FUN_80131840, which steps rows (1008, 231/229/222), (800, 190), (816, 205) through its own phase
words from fixed tables 0x8014A3A4/0x8014A444/0x8014A4C4/0x8014A5A4. The new-game opening's cycled rows (the "title"
rows in the earlier note: x 800/816 rows 190/205, x 1008 rows 210..255) are these A00 owners; a
temporary upload log showed them as LoadImage uploads of 16x1 rows each tick.

## Evidence
Run `h3` (fix, fps60 on) and `h4` (fps60 off), same plan. Water box mean change per present: 5.6, 1.2, 1.1, 5.1,
5.5, 0, 0, 3.8, 4.3, 2.5, 2.4 (before: the 8.4 and 8.1 steps landed on one present). At in-between 8, 0 of 216 moving
tiles equal the newer real (before: 216); its mean distance from the midpoint of its neighbours is 1.5 against 5.7
and 6.3 from each (the wave's motion is the rest). `h3_water_7_8_9.png`: the middle crop is the cross-fade. The real
presents of `h3` are byte-identical to `h4`'s, and guest RAM at the two marks is identical with fps60 on and off.

With the declared cycles (`scratch/clutcycle/w1`, same plan, `debug clut`): the water box changes 5.6, 1.2, 1.1,
5.1, 5.5, 0, 0, 3.8, 4.3, 2.5, 2.4 per present, as `h3`; in-between 8 is 1.0 from its neighbours' midpoint against
3.8 and 4.3 from each (`scratch/clutcycle/w1_water_7_8_9.png`). Over the boot, the new-game opening and A0D, the rows
cross-faded are exactly the rows the generic blend changed in run `title`: (816, 250); A00's (800, 190), (816, 205)
and x 1008 rows 210, 222, 227, 229, 231, 232, 233, 235, 253..255; SOP's (688, 510/511). No other CLUT change blends.

## Producer-built in-betweens
A composed in-between (a screen whose draw ran no replay section) keeps the newer entries. No Tomba producer screen
checked cycles a palette: on the pause menu over A0D (`tap start` at 2850 after `warp 13/0`), raw VRAM is unchanged
across 12 consecutive frames, the water cycle included (`scratch/wave0058/menu`, `menu_pause.png`). The new-game
opening's cycled rows (A00, above) are replayed and blend through the same path (`scratch/clutcycle/o1`). The save, load and map screens were not checked.
