# 0074 — The wide margin shows retail's draw order hiding the village ladder

Status: open. Owner: `game/render/overlay_ground_gt3gt4.cpp` (`OverlayGroundGt3Gt4`, A00 FUN_801401B8).

## Observed
User report 2026-11-20 ("Ladder got cropped", x 81-97%, y 48-87%): the ladder leaning on the hut right of the
fire tower in area 0 loses the middle of its right rail behind the hut's front wall; its feet show again in front.

## Cause
Ladder and hut are both A00 ground cells drawn by `OverlayGroundGt3Gt4`. The override differential on 0x801401B8 at
4:3 (`PSXPORT_OVERRIDE_DIFF=0x801401B8`, the report's state) matches the guest body in every RAM and scratchpad
byte: the buckets are retail's. The wall's quads sort nearer than the rail's middle quads, a painter's-sort error
retail keeps off screen; the 16:9 margin shows that part of the ladder.

## Proper fix
Per-pixel depth for the record path's 3D draws, so overlapping polygons resolve by depth instead of OT bucket.
