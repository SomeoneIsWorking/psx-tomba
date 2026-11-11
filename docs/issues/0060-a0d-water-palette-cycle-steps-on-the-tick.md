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
change still lands on its tick. The generic blend of every changed CLUT (psxport 7a9937b9) is removed.

The scripted routine is found by its code: every overlay LoadImage call site (`jal 0x80081218`) whose 44 instructions
around the call match A0D's with immediates masked (`scratch/clutcycle/match_cycle.py`), its descriptors and count read
from the routine (`cycle_consts.py`) and its decompilation. Declared (routine, descriptors, count): A00 FUN_80115B88
0x80147D84 x12, FUN_8013C0BC 0x8014ABDC x1; A01 FUN_801112C0 0x801352CC x6, FUN_8012D6B0 0x80137B3C x1; A02
FUN_8011321C 0x80129074 x9 (frame 0 only unless 0x800BF8BE is -1); A04 FUN_80123B90 0x80141584 x1, FUN_80116884
0x8014097C x1 or 0x80140BAC x2 by bit 0x10 of 0x800BFE56; A06 FUN_801271E4 0x80147A30 x1, FUN_80115BC8 0x80142A5C x1 or
0x80142F50 x5 by bit 0x40; A07 FUN_80113C20 0x80132B14 x5; A08 FUN_801152F0 0x80142EA8 x25, FUN_8012579C 0x8014579C
x1; A09 FUN_8010A060 0x8010E0DC x9, FUN_8010B3A8 0x8010E260 x1; A0A FUN_8010B9AC 0x8011937C x5; A0D FUN_8010B288
0x801175E0 x1; A0E FUN_8010BEB0 0x801181EC x1; A0F FUN_8010DF5C 0x8011AE60 x12; SOP FUN_8010C6A0 (and its copy in
the grid draw 0x8010C26C) 0x8010D2FC x3. Objects stepping a row through their own phase word from a fixed table:
A00 FUN_80131840 (rows (1008, 231/229/222), (800, 190), (816, 205); tables 0x8014A3A4, 0x8014A444, 0x8014A4C4,
0x8014A5A4), A00 FUN_8012C910 ((880, 213), 0x80149EE4, three frames every fourth tick), A06 FUN_80117CF4 ((1008, 208) or (1008, 192), 0x80144D88, order {0,1,2,3,2,1} at 0x80144E08), A08
FUN_80127C58 ((1008, 231), 0x801459E4, order at 0x80145A64).

The other overlay LoadImage callers are not cycles and stay undeclared: a greyscale pass (StoreImage, luma, LoadImage)
A01 FUN_801321C4, A05 FUN_80135ED8, A06 FUN_8013CBA0, A07 FUN_8012D880, A08 FUN_8012A638 (one routine); a tint toward a
colour by a weight A04 FUN_80117C58 and A05 FUN_80134F1C; A0L FUN_801129D4, a double-buffered
streaming upload. A0B, A0C, A0G..A0K call LoadImage nowhere.

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
