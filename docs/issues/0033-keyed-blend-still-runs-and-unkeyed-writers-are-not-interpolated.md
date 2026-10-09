---
id: 0033
title: Keyed blend's base still feeds Tomba's composer, and unkeyed writers are not interpolated
status: open
labels: [render, fps60]
---

Every keyed producer has a state render (`codemap.md`, State producers). Two things remain.

Keyed blend. `FramePresenter::presentRecords` (psxport `runtime/psx/frame/frame_presenter.cpp`) runs `keyedBlend`
and hands its result to `composeFrame` as the base. The composer replaces every entry bound to a producer with a render,
so for Tomba the blended positions of produced entries are discarded; only keyed entries the composer leaves (an
ambiguous object) still take keyed blend's positions. No title switch is needed: `keyedBlend` is deleted once Crash Bash
and Toy Story 2 have state renders, and the base becomes the shown record.

Unkeyed writers, presented as the guest drew them: UI 0x8007E6DC, billboard 0x8003C2D4, 0x8002AB5C,
0x80106824, the fade 0x8007E1B8 and 0x8007E9C8. They are not producers; converting one means giving it an
object and a render.

Not covered by tests: a scrolling grid or moving model list in the running product. Both recorded areas have an
idle camera, and steering gameplay to move it is not allowed; a title debug option that moves the camera would
show it.
