---
id: kanban-077
title: Camera blocked by geometry vanilla does not show — 2 spots, stage GAME (USER)
status: open
labels: [bug, render]
features: [F-RENDER]
---


USER 2026-08-06, two spots, stage GAME 0x8010637C, coordinates are the RmlUi HUD camera triple (0x1F8000D2/D6/DA):
 (1) cam X 13029 Y -2872 Z 7161 — 'geometry blocking the screen ... that geometry IS there, but vanilla culls this'
 (2) cam X 20161 Y -1923 Z 8268 — 'the water way blocks the camera'
MECHANISM UNIDENTIFIED. Do NOT invent a distance/ID/alpha cull — that is a bandaid with no ground truth.
BLOCKED ON TWO INSTRUMENT DEFECTS, both measured 2026-08-06 (see the two sibling cards):
 - the report carries no AREA/SUB, and the HUD does not print one, so the coordinate cannot be resolved to a place;
 - psx_render draws no world geometry, so the pc-vs-psx reference comparison that would split 'vanilla culls it' from 'the object should be unloaded' cannot be run at all.

## 2026-11-03: a candidate spot

`warp 0 5` (area 0, entry 5) puts the camera behind the tower with the A00 windmill lattice sweeping across the
left of the picture; camera `0x1F8000D2/D6/DA` = (22291, -3180, 7134)
(`scratch/tomba2/evidence/kanban077_area0_entry5.png`). Its large polygons are written by `0x801401B8` and A00's
plain GT4 `0x801467BC`; none exceeds the GPU's 1023x511 primitive limit, so the console would draw them too. Whether
retail shows this view needs the full-console reference driven to the same warp; the console host has no RAM write
command yet, so the warp cannot be applied there.

Decided 2026-11-03: at `warp 0 5` the lattice is the game's own draw, not a port defect. A 4:3 run
(`aspect=0`, unpaced) with `PSXPORT_OVERRIDE_DIFF=0x801401B8,0x801467BC,0x801465EC` sampled 41 / 959 / 959 calls
across free roam and the warp. The leaves' mismatch onsets differ only in `v1`, a void body's scratch result, never in
a RAM or scratchpad byte. `OverlayGroundGt3Gt4::entityLoop` also left different GTE CR0-7: it loaded the camera block
0x1F8000F8 for an empty list, where the guest returns before that load and keeps the caller's matrix for whatever GTE
work follows. The native now skips the load on an empty list; a run differencing 0x801401B8 alone (33 of 1106 calls
sampled) shows call 1 matching and the first
onset only in `v0`/`v1` with no GTE, RAM or scratchpad difference
(`scratch/tomba2c/evidence/kanban077_entityloop_diff_after.txt`). Calls after an onset are counted, not itemised, so
this shows the first divergence of each, not every call. Entry 5 is a real area 0 entry, so a player arriving there
sees the same view. The user's two spots stay open: their area is still unknown.
