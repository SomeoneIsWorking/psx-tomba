---
id: kanban-077
title: Camera blocked by geometry vanilla does not show — 2 spots, stage GAME (USER)
status: open
labels: [bug, render]
---


USER 2026-08-06, two spots, stage GAME 0x8010637C, coordinates are the RmlUi HUD camera triple (0x1F8000D2/D6/DA):
 (1) cam X 13029 Y -2872 Z 7161 — 'geometry blocking the screen ... that geometry IS there, but vanilla culls this'
 (2) cam X 20161 Y -1923 Z 8268 — 'the water way blocks the camera'
MECHANISM UNIDENTIFIED. Do NOT invent a distance/ID/alpha cull — that is a bandaid with no ground truth.
BLOCKED ON TWO INSTRUMENT DEFECTS, both measured 2026-08-06 (see the two sibling cards):
 - the report carries no AREA/SUB, and the HUD does not print one, so the coordinate cannot be resolved to a place;
 - psx_render draws no world geometry, so the pc-vs-psx reference comparison that would split 'vanilla culls it' from 'the object should be unloaded' cannot be run at all.
