# 0076 — The pig's basket rests at the cliff foot

Status: closed (2026-11-26), retail behaviour. Owner: guest A00 FUN_80117228 (tomba2-decomp
`docs/notes/a00-pig-basket.md`).

## Observed
User report 2026-11-21 ("This is not supposed to be here, a pig carries this usually and when you disturb the pig,
this item flies upwards"): after the pig is disturbed, its basket sits on the ground at the foot of the cliff beside
the door slot. The device's own render of the reported frame (psx.png) shows the same.

## Cause
None in the port. The report state has quest byte 0x800BF9E5 = 4 and the basket at (14816, -1375, 8416), variant 1,
+0x5E = 1, at rest. FUN_80116CC4 drops the flown basket from y -3450 until it lands, stopping it at y -1375. That
hard floor is the game's own constant, and the basket then waits there for scene event 6 to move the quest on.
The drop runs as guest code (FUN_8004766C). The port runs only the bounds cull natively, and that test decides when
the basket moves, not where it stops.
