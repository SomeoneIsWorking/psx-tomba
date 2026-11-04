# 0052 — areas 3, 4, 7, 10, 11, 13 and 15 show their in-betweens whole

Features: F-FPS60.

Status: open (2026-11-04); areas 7, 10 and 15 owned. Owners: `UnlitModelEmitter` (kA0a, kA0f), `QuadRtptSubmit`,
`ObjectDraws::hangingStrip`, `AreaMap`, `TileGridLayer`, `GuestKeyedEmitters`.

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

## Area 10 fixed
A0A's pair 0x8010F97C/0x8010FD74 is `UnlitModelEmitter` kA0a (depth from the SZ registers, a byte UV scroll per
flag bit), A0F's 0x80114A5C/0x80114E70 kA0f, and 0x8003B320 a `QuadRtptSubmit` state producer keyed by the
drawer's walked object. A0A's chain drawer 0x80114E64 calls 0x8003B320 once per segment, 35 times for its tallest
strip, past `CallSiteObjects`' 15 calls a site, so it is an `ObjectDraw` whose calls are its parts.
`warp 10/0` now shows 21 of 21 in-betweens at 0.41-0.54 (`scratch/tomba2e/r4/reduced/shares.txt`).

## Areas 7 and 15 owned
`warp 15/0` reached two states across runs. Its writers: A0F's own hanging strip 0x801175C0 (A0A's sway branch,
up to 64+ segments, so `ObjectDraw` parts are no longer capped), the wire 0x80116ABC over 0x80112D10's shimmer
lines and the lens flare 0x801143C4 over 0x80114288 and 0x80027A4C (both `GuestKeyedEmitters`). With them the
journal is clean, but the in-betweens sat at 0.80-0.90: the purple backdrop's byte UV scroll was a Held input, so
it stepped whole. The scroll words are now wrapped inputs (a byte or a half-word period), and area 15 sits at
0.69-0.80 (`scratch/tomba2e/r7/reduced/`): geometry is halfway; the red left in `whole_small.jpg` is the backdrop
layer whose scroll moves one texel a frame, which a byte UV cannot show at a half.

`warp 7/0`: 0x801140A0 is A02's area-map body turned a quarter (`AreaMap` layout 1), 0x8012F938 is the tile grid
with its walk one map row early (`TileGridLayer::emitRowLead`), and 0x80117AB8's caller 0x80117D4C is a
`GuestKeyedEmitters` leaf (its other caller 0x80117FE0 was not reached and stays guest code). The journal is clean; Tomba idles there, so moving in-betweens are 0.46-0.64
and the 1.00s are the map marker's blink, a 14x14 cell (`scratch/tomba2e/r8/reduced/`).

`warp 10/0` in its second state held every in-between whole on 0x80027A4C stamps reached through MAIN 0x800328BC,
the node stamp the area effect leaves call; it is now a `GuestKeyedEmitters` leaf and the area sits at 0.50.

Proof run `scratch/tomba2e/r12/reduced/` (`shares.txt`, `owners.txt`): areas 2, 7, 10 and 15 log no unowned or
overwriting writer; area 10 is 0.50-0.51, area 15 0.69-0.79 (the texel-step backdrop above), areas 2 and 7 move at
0.40-0.65 with the 1.00s on the map marker's blink.

Still unowned (`scratch/tomba2e/r9/reduced/shares.txt` and its journal; area 13 again in `scratch/tomba2e/r10`):
- `warp 3/0`: 0x8010F1A0, 0x8010F968, 0x80110290, 0x80110094 (A03 emitter copies), 0x80109EEC (0x64), 0x80110B00 (0x3A).
- `warp 4/0`: 0x8013F418/0x8013F6E4 (A04 lit pair conditioned on 0x1F800207 and 0x1F80009C), 0x8013AD90 (0x60).
- `warp 11/0`: the journal names the render dispatcher 0x8003C048; the writer it dispatched is A0B 0x801110BC
  (block 0x80111334), a dust field of 513 points from an LCG seeded by node+0x50, wrapped around the camera
  (0x1F8000D2/D6/DA plus 0x1F800104-0x108), RTPS-projected and linked as 1x1 or 2x2 white rects at bucket 0x100.
  It keeps only x < 0x140, so the margins get no dust either. Its producer is a native body over `EmitMemory` with
  the scene camera blended and the cull widened to the draw window.
- `warp 13/0`: 0x80112DE4/0x801131B0 (A0D pair; its bit-4 branch calls rsin). Its 0x80027A4C stamps are owned by
  A0D 0x80110C14.

## Next
Per area, decompile each writer and give it the producer its pattern takes (`docs/producers.md`): emitter copies
become `UnlitModelEmitter`/`LitModelEmitter` variants, leaves that link their own packets take `quad_state.h`.
Start with 0x8003B320, the one MAIN writer shared by three areas.
