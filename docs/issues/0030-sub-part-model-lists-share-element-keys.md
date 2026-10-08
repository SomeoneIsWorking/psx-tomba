# 0030 — sub-part model lists share element keys

Status: closed. The premise was wrong; the duplicates came from A01's third emitter pair, now native.

## Observation

Since psxport 7e8d4fb1 a key bound to two packets is ambiguous and drawn uninterpolated. This issue
claimed the A01 (0x80132DC0), A05 (0x801362CC) and A08 (0x8012A9DC) drawers call two native GT3 and two
native GT4 emitters inside one `Render::subPartWalk` sub-part, so record i of the two lists shares a key.

The producer census now counts keys used more than once in a record (`census`, "duplicated keys").
Shipping settings (16:9, fps60), `newgame`, `run 990`, then `warp N`, `run 300`, `press right`, `run 60`:

| segment | duplicated keys |
|---|---|
| intro f0-990 | 0 |
| area 0 | 0 |
| area 1 | 1960, all `cmdListDispatch` 0x8003CDD8; first (0x8003CDD8, 0x800F407C, 0, 0) written by 0x80132DC0 |
| area 8 | 0 (12 more across the 1 to 8 warp, still area 1's lists) |

## Cause

Each drawer calls one GT3/GT4 pair per branch, never two:

- byte 0x800B8873 set: the area's lit pair (A01 0x801316A8/0x80131BB0, A05 0x8013544C/0x8013590C,
  A08 0x80129BAC/0x8012A06C);
- else A01: byte 0x800B8816 set, the resident pair through 0x800803DC; else A01's own unlit pair
  0x80132690/0x801329C4;
- else A05: the resident pair, or A05 0x8013AC90/0x8013AF0C; A08: its plain pair 0x80140FBC/0x801411D8.

0x800803DC also calls one pair. The duplicates are A01's third branch: 0x80132690/0x801329C4 ran as guest
bodies, which name no element, so every primitive of a render command carried
(0x8003CDD8, record, 0, 0).

## Fix

`UnlitModelEmitter` owns A01's pair (`kA01Cue`: SOP's body keeping every code byte, staging the SZ words at
its 16-byte frame for every record, depth-cueing each corner by SZ / 4), naming each record
`modelElement(list, index)`. `model_element.h` is unchanged.

Evidence: the census over the same segments reports 0 duplicated keys in all four; area 1 draws 46 more
primitives at 16:9 (the margins the 320 cull dropped). `tests/test_authentic_model_emitters.cpp` runs both
retail bodies against the native; a variant with the cue removed fails it. psxport
`tests/test_producer_census.cpp` covers the duplicate count.
