---
id: kanban-080
title: Widescreen margin re-include is DEAD CODE — Cull::objectCull has no caller
status: closed
labels: [bug, render]
features: [F-WIDE]
---

## Cause

`Cull::objectCull` (`game/render/cull.cpp`) had no caller: every camera-relative wrapper and `Actor::boundsCull`
call `performBaseCull`/`performBaseCullFramed` directly. Its only effect, `MarginRenderer::collect`, filled a set
that `MarginRenderer::flush` cleared every frame without a reader, so the re-include never reached the picture.

## Fix

Deleted `Cull::objectCull` and `game/render/margin_render.*`. Margin geometry is the margin producers' work, tracked
in 0028; the cull keeps the guest's own limits.
