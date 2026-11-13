# 0068 — Areas 11, 12 and 15: the arrival event never releases Tomba

Status: open (2026-11-13).

## Reproduction
`warp 11/0` (or `warp 11/1`, or `spot` into areas 11, 12 or 15) from a paused boot stepped to 1050, unpaced,
`ires=0`, `fps60=1`, sink 3840x2160 (`scratch/m0062/drive/p4.py`, runs `w11`, `pc11`, `pc12`, `pc15`).

## Observed
Ten frames after arrival Tomba enters mode `G+4 = 4, G+5 = 0x22` and stays there for the whole run (600+ frames):
his position never changes, `G+6` cycles 2, 1, 0, 1, and `0x800BF80E` blinks between 0 and 1. In area 11 he hangs
in the air over the snow.

## Known so far
Mode 0x22 is MAIN `FUN_800658E4` (from mode table B `0x80058F5C`): it plays Tomba's animation, sets `0x800BF80E`
when it ends, and holds until the event signal `0x1F800184` is 1 or 2. Its writer in A0B is the arrival event
`FUN_8010F144` (A0C `0x80110474`, A0F `0x80110DA4` write the same mode), which loads `0x8011A544[area - 10]` and steps on
the cutscene byte `0x800BF80F` and `0x800BF80E`. The value that never arrives is not traced.
