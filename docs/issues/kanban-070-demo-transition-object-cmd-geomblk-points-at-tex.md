---
id: kanban-070
title: DEMO transition: object cmd geomblk points at texture data, emitter submits ~125k garbage quads
status: closed
labels: []
features: [F-RENDER]
---

## Closed (2026-11-03)

Not present on HEAD. `PSXPORT_NO_FMV=1`, `PSXPORT_AUTO_SKIP=0`, no input, unpaced, from boot through the title into the
attract demo and back (DEMO `sm[0x48]` 7 until frame 1664, 0 at the transition frame 1665, 1 from 1666).
`PSXPORT_PRODUCERS=1` arms psxport's producer census, which counts every primitive of every sealed frame record, and
`census` was reported over the control channel 164 times between frames 1401 and 1900. The cumulative count went from
1,127,459 to 1,145,107: 17,648 primitives over those 500 frames, at most 701 in one interval (2 frames, ending f1823),
and 5 a frame across 1655-1680. A 125k-quad emission in any frame of the window would put at least 125,000 into one
interval. Data: `scratch/tomba2/evidence/kanban070_census_f1401_f1900.txt` (frame, cumulative primitives); the demo
picture: `kanban070_attract_demo.png`.

An earlier reading of the packet-pool cursor `0x800BF4F4` sampled every 0.2 s is withdrawn: it took the offset modulo
the 81,920-byte pool, so an overrun could not show, and it never sampled frame 1665. The geomblk diagnostic and the
substrate emitter this card was filed from are both retired.
