---
id: kanban-066
title: A0L phase byte 0x800BFA55 gates the area-21 jet effect (FUN_8010C1D8) — need a scene where it reaches >= 4
status: open
labels: [render]
---


FUN_8010C1D8 (A0L overlay, area 21) returns immediately unless *(u8*)0x800BFA55 >= 4. In the standard area-21 capture (warp 21; skip 600) it reads 1, so the effect draws NOTHING and the producer cannot be pixel-verified there — a 0-px A/B would be indistinguishable from a broken port (a single-instant pixel A/B cannot tell the two apart). Its record table at 0x801154E0 is a SINGLE 36-byte record (rec0 [+4] = 0xC02E0000, terminator bit set). NEXT: find what advances the phase, then reach that scene to verify a future producer there.
