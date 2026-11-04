# 0052 — areas 3, 4, 7, 10, 11, 13 and 15 show their in-betweens whole

Features: F-FPS60.

Status: open; every listed area owned and shown halfway unpaced, but no real-time walk from boot and no pacing
judgement yet. Owners: `UnlitModelEmitter` (kA0a, kA0f, kA0d, kA03),
`QuadRtptSubmit`, `ObjectDraws::hangingStrip`, `AreaMap`, `TileGridLayer`, `GuestKeyedEmitters`, `DustField`,
`FogModelEmitter`, `SubdividedGround`.

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
0.69-0.83. Measured split (`scratch/tomba2e/w3/reduced/a15_hue_split.txt`, `d15/reduced/`): over 24x24 cells with no
purple the in-betweens sit at 0.37-0.63; the purple veil sits at 0.74-0.83. The veil is two overlapping layers of the
kA0f pair. Records with flag 4 scroll U by the byte 0x8012092E, 2 texels a logic frame in this run, and the in-between
gets the exact middle (logged scroll at render: reals 18 then 16, the in-between 17). Records with flag 8 scroll by 0x80120938,
1 texel a logic frame, and a byte UV has no half, so `ListStateProducer`'s wrapped blend rounds that layer to the later
frame. One layer at 0.5 and one at 1.0 over the same pixels gives about 0.75, as measured; nothing else in the area is
off halfway. The flag-8 layer is a limit of the packet's integer UV, not a missing producer.

`warp 7/0`: 0x801140A0 is A02's area-map body turned a quarter (`AreaMap` layout 1), 0x8012F938 is the tile grid
with its walk one map row early (`TileGridLayer::emitRowLead`), and 0x80117AB8's caller 0x80117D4C is a
`GuestKeyedEmitters` leaf (its other caller 0x80117FE0 was not reached and stays guest code). The journal is clean; Tomba idles there, so moving in-betweens are 0.46-0.64
and the 1.00s are the map marker's blink, a 14x14 cell (`scratch/tomba2e/r8/reduced/`).

`warp 10/0` in its second state held every in-between whole on 0x80027A4C stamps reached through MAIN 0x800328BC,
the node stamp the area effect leaves call; it is now a `GuestKeyedEmitters` leaf and the area sits at 0.50.

Proof run `scratch/tomba2e/r12/reduced/` (`shares.txt`, `owners.txt`): areas 2, 7, 10 and 15 log no unowned or
overwriting writer; area 10 is 0.50-0.51, area 15 0.69-0.79 (the flag-8 veil layer above), areas 2 and 7 move at
0.40-0.65 with the 1.00s on the map marker's blink.

## Areas 3, 4, 11 and 13 owned
- `warp 13/0`: A0D's pair 0x80112DE4/0x801131B0 is `UnlitModelEmitter` kA0d, kA0a's body whose flag-4 records move
  each corner's SY by rsin(phase 0x8011CC68 + model x * 4) >> 10 and sort 0x80 buckets back; the phase's low half is
  a wrapped input.
- `warp 11/0`: A0B 0x801110BC is `DustField`. The 513 points come from `CameraLattice`, the cube rain already used,
  now one owner for both; the camera is blended and the guest's SX < 320 cut is the draw window, so the dust fills
  the margins.
- `warp 4/0`: A04's pair 0x8013F418/0x8013F6E4 is `FogModelEmitter` (AVSZ fog into DPCT/DPCS, hidden on 0x1F800207
  15-17 or 0x1F80009C); 0x8013AD90 is `DustField::drawSnow`, the lattice from the fixed seed 0x12D687 with node+0x52
  + 1 points, each kept only inside the cave mouth, skipped whole when the pool has under 0x8000 bytes left
  (`PacketPool::hasRoom`).
- `warp 3/0`: 0x80110290 is `SubdividedGround` with 0x8010F968 (fade and 2x2 split on screen) and 0x8010F1A0 (model
  quad split twice and projected again) folded in, since only it calls them; 0x80110094 is `UnlitModelEmitter` kA03
  (two-sided on flag bit 0). 0x80110B00's only caller 0x801113B4 and the HUD 0x80109EEC are `GuestKeyedEmitters`,
  whose overrides and renders both come from its one `kEmitters` table.
  0x80110094 and the leaves of 0x8010F1A0 and 0x8010F968 pick the compression band from depth - 0xE9 (`addiu -0xE9`
  at 0x801101F0, 0x8010F730, 0x8010FD48; `kA03BandBias`), the averaged paths of 0x80110290 from depth (0x801106F8,
  0x80110834). A depth under 0xE9 there gets band -1 and no bucket, and depths in [k*0x400, k*0x400+0xE9) stay in
  the band below.
  The guest links a far subdivided quad whose bucket is out of range one slot before the OT; `ListStateProducer`
  holds that word in its host OT, and no OT walk reaches it.

Proof run `scratch/tomba2f/v3/reduced/` (`shares.txt`, `display60.txt`, `off_summary.txt`, `a04_snow_points.txt`,
`owners.txt`, `a*_small.jpg`), unpaced, ires 0, fps60, 3840x2160, `warp 3/0` dumped 240 frames in (mid-ride, before
the crash into "Failed..." that a 330-frame wait catches), the others 330: on the simulated 60.00 Hz display every
present is composed (64/64), one repeat, no skip or backward step; the journal holds only the warp's own held
intervals. Whole-frame shares after the first sample: area 3 0.45-0.58, 4 0.47-0.55, 11 0.47-0.54, 13 0.48-0.56 with
one 0.21 (a creature spawning: no earlier state to blend from, `a13_3340_3342.png`). Per 24 px tile the off-halfway
share of changed pixels is 7% (11), 14% (13) and 33% (3); a tile share is a pixel-difference measure, so textured
walls moving a few texels sit anywhere in it. Area 3's tiles at 0 or 1 sit in the top corners of the margins, where
near cave-wall pieces appear between two reals (`a03_top_1866_1867.png`), and at the bottom centre, where ties turn
black at the SZ < 0x514 corner fade (`a03_near_ground_1769.png`). Area 4's snow points are too small and fast for
tiles; matched point to point, in-between snow sits at a median 0.50 (quartiles 0.30-0.69) of the way from the
earlier to the later real, against 1.00 for the later real itself. Area 3's cave walls, area 4's snow and scenery and
area 11's dust reach both 16:9 edges.

## Next
- Trace the area 3 margin wall pieces that appear between reals: the leaf of 0x8010F1A0 drops pieces whose farthest
  depth is under 0xE9 (band -1), which in 4:3 the corner fade hides; check whether that is the margin drop.
- One real-time walk from boot through areas 3, 4, 11 and 13 with pacing judged on a quiet host.
