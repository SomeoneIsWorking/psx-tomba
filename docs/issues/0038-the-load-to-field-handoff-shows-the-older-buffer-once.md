# 0038 — the load-to-field hand-off shows the older buffer once

Features: F-FPS60.

Status: open.

## Observed
Real-window play check, `warp 1`, fps60 on (2026-10-16): `records: backwards=1` at record present 126, the
area-load end (f686). Shown records 124, 123, 124: records 125 and 126 draw nothing and write nothing but the
2x1 self-copy fence, while the display flips each pass.

## Traced
The displayed VRAM buffer at present 126 hashes equal to the one shown at present 124 (record 123's full-screen
fade colour); record 124's differs. So the device holds record 123's picture there, and the presenter shows what
the device holds: the step back is the guest's display flip over two passes that drew nothing, in the
`PresentState::Present` arm of `game/core/frame/frame_driver.cpp:finishPass`. Both pictures are full-screen
fade frames, so one present steps the fade back one level.

## Gap
Not established whether the retail loop makes the same two empty passes there or the port's frame boundary does:
compare the pass sequence at the A01 load end against an oracle run. If the retail game does it, the §6
"never backwards" bar fails on faithful output.
