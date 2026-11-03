---
id: kanban-070
title: DEMO transition: object cmd geomblk points at texture data, emitter submits ~125k garbage quads
status: closed
labels: []
features: [F-RENDER]
---

## Closed (2026-11-03)

Not present on HEAD. `PSXPORT_NO_FMV=1`, no input, from boot through the title into the attract demo and back
(DEMO `sm[0x48]` 7 from frame 764 to 1664, the transition at 1665): the packet-pool cursor `0x800BF4F4`, sampled 475
times over 185 s, peaked at 65,940 of the 81,920-byte parity pool. A 125k-quad emission is at least 2.5 MB and would
overrun that pool. The geomblk diagnostic and the substrate emitter this card was filed from are both retired.
Shots: `scratch/tomba2/evidence/kanban070_attract_demo.png`.
