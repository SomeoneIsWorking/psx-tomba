---
id: 8
title: Most Engine *Faithful mirrors in engine.cpp have no callers
status: open
symptom: engine.cpp carries pc_faithful register-discipline twins of native scene methods that nothing reaches, and they travel along with every god-file extraction
state_items: S001
tags: tomba2,engine,dead-code,structure
created: 2026-09-18
updated: 2026-09-18
---

## Finding

While landing the FieldTransition extraction (game/scene/field_transition.*), a whole-tree search
showed `Engine::fieldTransitionFaithful` and its four `transition*Faithful` workers had no caller
anywhere in `game/` or `titles/`. They were deleted from the extracted unit rather than moved.

The same search over the remaining `Engine::*Faithful` members of `game/core/engine/engine.cpp` found
callers only for `fieldFrameFaithful`, `stageBodyFaithful`, `sceneEventFifoFaithful`, and
`startBinStageFaithful`. `areaModeDispatchFaithful`, `fieldFrameXFaithful`, `fieldRunFaithful`,
`fieldRunXFaithful`, `frameStartTickFaithful`, `modePerFrameDispatchFaithful`,
`postRenderTickFaithful`, `sceneRenderListBuilderFaithful`, `sceneStateStepFaithful`,
`submitPage810cFaithful`, and `submode1Faithful` matched zero call sites by name.

## Next step

Confirm each zero-caller twin is unreachable through any dispatch table or override registry, delete
it with its declaration and doc block, and ratchet the `game/core/engine/engine.cpp` policy cap in
`CMakeLists.txt` by the removed lines. Do this as its own slice, before the next extraction moves
more dead bodies into new owners.
