---
id: 0033
title: Keyed blend's base still feeds Tomba's composer
status: open
labels: [render, fps60]
features: [F-FPS60]
---

Every keyed producer has a state render (`producers.md`, State producers). One thing remains.

Keyed blend. `FramePresenter::presentRecords` (psxport `runtime/psx/frame/frame_presenter.cpp`) runs `keyedBlend`
and hands its result to `composeFrame` as the base. The composer replaces every entry bound to a producer with a render,
so for Tomba the blended positions of produced entries are discarded; only keyed entries the composer leaves (an
ambiguous object) still take keyed blend's positions. No title switch is needed: `keyedBlend` is deleted once Crash Bash
and Toy Story 2 have state renders, and the base becomes the shown record.

The writers listed here before are producers (`producers.md`, State producers): the fade 0x8007E9C8, the 2D sprite
writers 0x8007E1B8 and 0x8007E6DC (which the DEMO menu 0x80106824 draws through), billboardEmit 0x8003C8F4 (which
0x8003C2D4 hands to), and the drop shadow 0x8002AB5C over the packed-mesh writer 0x80027768. Tested by
`tests/test_writer_producers.cpp` (composeFrame at t = 1 and 0.5) and, local-only, `tests/test_authentic_writers.cpp`.

Not covered by tests: a scrolling grid or moving model list in the running product. Both recorded areas have an
idle camera, and steering gameplay to move it is not allowed; a title debug option that moves the camera would
show it.
