# 0028 — margin pop-in from the 4:3 screen-edge cull

Status: open. Native X gates, the model emitter copies that match a native, the tile-grid skies and the
A0B backdrop fixed; the emitter bodies that differ from every native remain.

## Reproduction

Record path, fps60 off, `newgame`, `run 200`, `warp N`, `run 300`, then `press right` / `press left`
with a `shot` every 2 frames (`scratch/bars/tools/popseq.py`).

- Area 0 at 21:9 (`scratch/bars/pop0u/`): houses, fences and the windmill are absent from the
  margins until they reach the buffer; ground in the margins is black.
- Area 8 at 21:9 (`scratch/bars/pop8u/l008..l014`): wall tiles behind the ladder in the right margin
  go black and the ladder drops out.

## Cause

Every Tomba! 2 screen-edge cull keeps a primitive only if some corner's unsigned SX is below 320
(`horizontal_visibility_cull.h`). On the record path `game/render/wide_window.cpp:drawRight` returned
320, because it only widened for the native-path wide engine, so a primitive wholly inside a margin
was never emitted, and an unsigned compare also drops the whole left margin.

## Fixed: the native X gates

`wide_window::drawWindow` is the draw window `[left, right)`: `[-M, 320 + M)` on the record path,
`[0, 320)` at 4:3. `Visibility` tests `(x - left)` unsigned against `right - left`, which is the
guest's compare when `left = 0`. Every native X gate goes through it: `OverlayGt3Gt4`,
`OverlayGroundGt3Gt4` (both A00), `QuadRtptSubmit`, `TextLabel`, `PerObjBillboard`.

Evidence: `scratch/bars/cull_a00_16x9_before.png` / `_after.png` (same frame; the before is a build
with the window pinned to `[0, 320)`; the inner 320 columns are identical in all 60 frames of the
pan), `cull_a00_21x9_before.png` / `_after.png`; `tests/test_horizontal_visibility_cull.cpp`
(`recordCanvasKeepsBothMargins`); 4:3 fps60-off `recordcheck` over the cutscene, an area 0 pan and
area 8: 2188 presents, all `mismatched=0`.

## Fixed: the area 8 model emitters

A08's four scenery emitters ran as guest bodies with a literal `sltiu 0x140`. They are two bodies:

- Lit GT3/GT4 (0x80129BAC / 0x8012A06C) are byte-identical, apart from local jump targets, to A05's
  0x8013544C / 0x8013590C and A07's 0x8012CDF4 / 0x8012D2B4. One native, `LitModelEmitter`
  (`game/render/lit_model_emitter.cpp`), serves all three overlays.
- Plain GT3/GT4 (0x80140FBC / 0x801411D8) are A00's 0x801465EC / 0x801467BC plus a UV scroll (flag bit
  2, s16 at 0x80145A6E). `OverlayGt3Gt4` takes the scroll word as a parameter; the GT3 copy scrolls uv0
  twice and never uv2, as the guest does. The guest's plain GT4 has no screen cull at all.

Both cull through `Visibility` over `wide_window::drawWindow`. Keys are unchanged: each override opens
`ModelObjectScope` (producer = entry, object = record list) when the walker opened
none, and names each record's element as the guest-body path did, so no new key can collide. The
duplicate-key count was not measured; there is no counter in the tree.

Evidence: `scratch/ws8/a08_16x9_pan_before_after.png` and `a08_21x9_pan_before_after.png` (the inner 320
columns are identical in 35/35 frames, the margins gain the walls and ladder);
`tests/test_model_emitters.cpp`; `tests/test_authentic_model_emitters.cpp` runs the retail A00, A05,
A07 and A08 bodies and the natives from one state on 2000 random record lists at 4:3: RAM, scratchpad,
v0 and sp byte-identical (with the local settings at 16:9 and the aspect not pinned, it reports the
first margin-kept packet, so it can fail). 4:3 fps60-off against a clean-HEAD build over the intro,
area 0 and an area 8 walk with exit fades: 17 screenshots, 17 RAM and 17 scratchpad dumps identical,
`recordcheck` 1745 presents, all `mismatched=0`.

## Fixed: the A01, SOP, A0B, A0C and resident model emitters

The common steps are one class, `ModelPacket` (`game/render/model_packet.*`); the bodies differ only in
what they add:

- A01 lit 0x801316A8 / 0x80131BB0 is A08's lit body plus two flag bits: bit 6 hides the record while
  0x1F80009C is set (GT3 after staging the depths, GT4 before), and bit 3 puts it 0x80 buckets deeper.
  `LitModelEmitter` takes them as `FlagBits`.
- Resident 0x8007FDB0 / 0x8008007C and SOP 0x801099B4 / 0x80109C80 are one unlit body that differs by
  data: scratch stage 0x1F800080 vs 0x1F800000, colour masks, depth-mode mask (&3 vs the whole byte) and
  the resident GT4 projecting corner 3 first. A0B 0x80112A24 / 0x80112CF0 and A0C 0x80113788 /
  0x80113A54 are SOP's body apart from jump targets. One native, `UnlitModelEmitter` with a `Variant`.
- A01 sway 0x80130838 / 0x80130D9C and cue 0x8012F8D8 / 0x8013000C are new bodies, `SwayModelEmitter`:
  vertex sway through guest rsin/rcos (called at their retail return addresses), UV scroll and the depth
  cue with the scratchpad LCG.

Keys are unchanged from the guest-body wrapper they replace (`ModelObjectScope`, then
`modelElement(list, index)`).

Evidence: `tests/test_authentic_model_emitters.cpp` runs 24 retail bodies (A00, A01 x6, A05, A07, A08,
SOP, A0B, A0C, MAIN) against the natives on 4800 random record lists at 4:3: RAM, scratchpad, v0 and sp
byte-identical, 0 fallback blocks. `tests/test_model_emitters.cpp` covers keys and the margin cull for
every family. 4:3 fps60-off against a clean-HEAD build on the same framework (intro f300/f600/f990,
area 0 and a pan, area 1 and pans both ways, areas 8, 11 and 12): 11 screenshots, 11 RAM and 11
scratchpad dumps identical, `recordcheck` 2638 presents, all `mismatched=0`. 16:9 before / after
(top / bottom) sheets: `scratch/ws9/wide/sheet_*.png`; in all 8 pairs the inner 320 columns are
identical and only the margins change: area 1's right margin gains rock, area 11 snow ledges and a crate.

## Fixed: the sky tile grids

`TileGridLayer::emitGrid` (A00 0x80115598 and SOP 0x8010C26C) walks the guest's columns
`[0, 320 + 32)`; it now walks the draw window plus the same 32 px slack. The map column wraps modulo
the map width, the guest's own rule, so the margins show the map's own cells. The SOP sky map is 22
columns (352 px): at 16:9 it repeats with a 352 px period, exactly its wrap. A wrapped map shows a cell
twice, so each tile's element is its lap (row lap << 16 | column lap); a lap-0 tile keeps its old key.

Evidence: `scratch/ws9/wide/sheet_sop990.png` and `sheet_a0.png` (the black margin bands are sky);
`tests/test_producers.cpp` (`testTileGridMargin`: 22 columns at 4:3, 29 at 16:9 starting at the
previous lap, every tile of a 22-column map with its own key, a repeated cell keeping its lap as it
scrolls).

## Fixed: the overlay emitter copies and the A0B backdrop

Every culling body in A02-A04, A06, A0A and A0D-A0L was compared against the natives, normalising j/jal
targets and branch offsets. The copies that match differ only by data and run on the existing natives:

- `OverlayGt3Gt4`: A07 0x801311D0/0x801313A0 and A0L 0x80112DEC/0x80112FBC are A00's plain pair.
- `OverlayGroundGt3Gt4` (`Variant`): A02 0x801246A4/0x8012496C and A07 0x8012C7E0/0x8012CAA8 pick the
  farthest SZ for flag mode 2 as well as 1; A0L 0x8010AA4C/0x8010AD40 too, plus 0x64 on GT3 mode 1 and GT4
  mode 2. Their walkers are guest code, so each list opens `ModelObjectScope`.
- `UnlitModelEmitter` (`Variant`):
  - A0A-A0F, A0H-A0J's flagged body (0 average, 1 farthest, else nearest; its GT4 drops a sorted quad);
  - A0G, A0H, A0I and A0J's near clamp;
  - A06 0x8013BA44/0x8013BD40 (lit depth code, hide bit 2);
  - A06 0x8013CF00/0x8013D1E4 (A01's third pair on the resident stage, with a U scroll).
- `LitModelEmitter`: A06 0x8013C0D8/0x8013C5B4, the lit body with hide bit 2.

The A0B backdrop is its tile grid, A0B 0x801141B0, A00's 0x80115598 without the 8-row V bias, copied
unchanged into A0A 0x801142EC, A0D 0x80116B9C and A0F 0x80116778. `TileGridLayer::emitUnbiased` owns all
four, so the grid walks the draw window like the other skies.

Evidence:

- `tests/test_authentic_model_emitters.cpp`: 70 retail bodies against the natives on 14000 random lists
  at 4:3, RAM, scratchpad, v0 and sp byte-identical, 0 fallback blocks. A quarter of the cases use a
  short focal length so the near clamp is reached. Removing any one data difference fails it: the clamp,
  the lift, the flagged GT4 drop, the flagged mode code, A06's hide bit (unlit and lit) and mode code, A06's U scroll,
  A0L's bias, A02's mode-2 pick.
- 4:3 fps60-off against a clean-HEAD build on the same framework: intro f300/f600/f990, then areas 0, 1,
  6, 8, 10-15 in one run and 16, 19, 21 in another (each `warp`, 200 frames, a 30-frame walk right). 16
  screenshots, 15 RAM and 15 scratchpad dumps identical; `recordcheck` 4179 presents, all `mismatched=0`.
  Areas 2, 7, 17 and 18 do not survive a warp on either build.
- 16:9 before / after (top / bottom) sheets: `scratch/keys/wide169/sheets/sheet_*.png`. In all 14 the
  inner 320 columns are identical; area 11's backdrop and snow ledges now fill both margins.

## Packet pool

80 KB per parity, no bound check in the guest. Peak of `0x800BF4F4` sampled every frame, fps60 off:

| run | 4:3 | 16:9 | 21:9 | 1024 wide (MatchSink 3840x720) |
|---|---|---|---|---|
| area 0 pan, 300 frames | 40656 (49.6%) | 48552 (59.3%) | 58656 (71.6%) | 61208 (74.7%) |
| area 8 walk, 110 frames | 48540 (59.3%) | 49100 (59.9%) | 50540 (61.7%) | 53260 (65.0%) |

No run comes near the end of the pool.

## Open

- Bodies that differ from every native still run as guest code with the 320 cull: A02
  0x80124DB8/0x80125100, A03 0x80110094/0x80110290, A04 0x801175FC/0x801178A0 and 0x8013D0F8/0x8013D568,
  A05 0x8013484C/0x80134B74 and 0x8013AC90/0x8013AF0C, A07 0x8012DBB4/0x8012DE60, A0A
  0x8010F97C/0x8010FD74, A0D 0x80112DE4/0x801131B0, A0E 0x80113AEC/0x80113DE0, A0F 0x80114A5C/0x80114E70,
  A0G 0x8010C3A4/0x8010C664, A0K 0x80114F1C/0x8011562C and 0x801163E0/0x80116708. Each needs reading and,
  where the steps differ, its own native. A0E's adds a world-X cut inside the projection.
- The A02, A07, A0H and A0I copies are proven only by the authentic test: those areas do not survive a
  warp.
