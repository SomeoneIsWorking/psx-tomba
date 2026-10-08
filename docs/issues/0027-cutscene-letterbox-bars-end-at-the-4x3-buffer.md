# 0027 — cutscene letterbox bars end at the 4:3 buffer

Status: closed.

## Reproduction

16:9, record path, fps60 off: `newgame`, `run 1265` (the intro shot after "And then...", Tomba on the
cliff). Expected: black bars across the whole presented width. Observed: the bars cover columns
54..373 of the 428-wide canvas; the margins show the scene above and below them
(`scratch/bars/bars_16x9_before.png`).

## Cause

`FUN_80026864`, slot type 1 of the 8-slot array at `0x80100400` (handler table `0x8009D314`), draws
the bars as two `FUN_8007FCC8` GP0 0x60 rects at x = 0, w = 320. They are primitives, not fills, so
the record canvas draws them only across their own 320 columns.

## Fix

`tomba2::render::LetterboxBars` (`game/render/letterbox_bars.cpp`) overrides `0x80026864` with the
guest's height machine and draws the rects from -M to 320 + M, M = `wide_window::marginColumns`. The
device clips them to the draw area, so VRAM is unchanged; at 4:3 M = 0 and the packets are the
guest's.

Evidence: `scratch/bars/bars_16x9_after.png` (same frame, bars span 0..427);
`tests/test_letterbox_bars.cpp`; 4:3 fps60-off `recordcheck` through the cutscene: 1428 presents,
all `mismatched=0`.

## Same shape: the screen fade

`FUN_8007E9C8`, the fade leaf behind every area fade, flash and the pause dim, fills x = 0, w = 320,
so at 16:9 the area-exit fade blacked the buffer while the margins kept the last frame
(`scratch/bars/pop8/l030.png`). `ScreenFade::draw` (`game/render/screen_fade.cpp`) now owns the leaf
and fills the draw window; its scratchpad staging and DR_MODE stay the guest's.

Evidence: `scratch/ws8/fade_16x9_before_after.png` (area 8 exit, frames l004/l012/l020),
`scratch/ws8/pause_dim_16x9_before_after.png`; `tests/test_screen_fade.cpp`;
`tests/test_authentic_screen_fade.cpp` runs the retail leaf and `ScreenFade::draw` from one state: RAM
and scratchpad byte-identical at 4:3, and only the fill's x and width differ at 16:9.

Not covered: `FUN_80034548` (reached from GAME 0x8010B398 via `FUN_800346BC`) draws a black
(0, 0, 320, 240) rect through `FUN_8007FCC8` into the far bucket, beside a 0x404040 fade; it looks like
a menu backdrop and would stop at the buffer the same way. Not seen in a run: the pause menu after a
warp crashes (0029).
