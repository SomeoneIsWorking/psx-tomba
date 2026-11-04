# 0052 — areas 3, 4, 7, 10, 11, 13 and 15 show their in-betweens whole

Features: F-FPS60.

Status: open (2026-11-04). Owners: none yet; each writer below is guest code with no producer.

## Reproduction
`warp <area>/0` from free roam for each area, unpaced, `ires=0`, `fps60=1`, `PSXPORT_PRESENT_SINK=3840x2160`, debug
`fps60dump`, 10 dumped in-betweens per area.

## Observed vs expected
Every in-between repeats the later real present (share 1.00) in areas 3, 4, 7, 10, 11, 13 and 15
(`scratch/tomba2d/walk/reduced/shares.txt`), and again over 32 in-betweens each in the second walk
(`scratch/tomba2d/walk2/reduced/shares.txt`). Areas 5, 12 and 20 sit at halfway, and 16-19 on the 8 moving in-betweens each has (the rest are still); areas 0, 6, 8, 9, 14 and 21 are off
halfway (issue 0054); area 1 is issue 0046; area 2 is issue 0051.

## Cause
The composer keeps an interval whole when a moving primitive has no producer. The walk's owner journal
(`scratch/tomba2d/walk/owners/*.jsonl`) names the writers, as `UnownedMoving` or `Overwrote` (a guest store over a
keyed packet), with the intervals each cost:

| warp | writer | GP0 | intervals |
|---|---|---|---|
| `warp 3/0` | 0x8010F1A0, 0x8010F968, 0x80110290 (A03, GT4; also overwrite 0x8008007C's packets) | 0x3C | 335 each |
| `warp 3/0` | 0x80109EEC (A03 tile), 0x80110094 (GT3), 0x80110B00 (G4 line) | 0x64, 0x34, 0x3A | 218, 138, 64 |
| `warp 4/0` | 0x8013AD90 (A04 rect), 0x8013F418, 0x8013F6E4 (GT3/GT4; the latter overwrites `cmdListDispatch` packets) | 0x60, 0x34, 0x3C | 284, 115, 115 |
| `warp 7/0` | 0x801140A0 (A07 tile), 0x8003B320 (MAIN, overwrites 0x8008007C), 0x80117AB8, 0x8012F938 (overwrites 0x8012CAA8) | 0x65, 0x2D, 0x2D, 0x7C | 158, 96, 84, 66 |
| `warp 10/0` | 0x8010F97C, 0x8010FD74 (A0A GT3/GT4), 0x8003B320, 0x80027A4C | 0x34, 0x3C, 0x2D, 0x2E | 354, 354, 346, 67 |
| `warp 11/0` | 0x8003C048 (MAIN rect) | 0x60 | 378 |
| `warp 13/0` | 0x801131B0, 0x80112DE4 (A0D GT4/GT3), 0x80027A4C | 0x3C, 0x34, 0x2E | 355, 348, 339 |
| `warp 15/0` | 0x80114E70, 0x80114A5C (A0F GT4/GT3), 0x80112D10 (A0F), 0x8003B320 | 0x3C, 0x34, 0x5E, 0x2D | 357, 346, 356, 349 |

0x8003B320 (MAIN, GT3 flat-textured) rewrites packets 0x8008007C keyed in areas 7, 10 and 15, so one owner there
covers three areas. Areas 5 and 6 also log unowned writers (0x8013AC90/0x8013AF0C, 0x80136F28; 0x80141A94) yet
compose most in-betweens, since those writers hold still on the dumped frames.

## Next
Per area, decompile each writer and give it the producer its pattern takes (`docs/producers.md`): emitter copies
become `UnlitModelEmitter`/`LitModelEmitter` variants, leaves that link their own packets take `quad_state.h`.
Start with 0x8003B320, the one MAIN writer shared by three areas.
