# Tomba! 2 draw emitters

The native GT3/GT4 model emitters and where guest code reaches them. In-betweens replay the draw
(`DrawReplay`, `docs/plans/replay-interpolation.md`). Owners are in `docs/codemap.md`.

| Guest address | Image | Owner | Reached from |
|---|---|---|---|
| 0x8007FDB0 / 0x8008007C | MAIN | `UnlitModelEmitter` gt3 / gt4 | 0x800803DC via 0x8003F698 |
| 0x801316A8 / 0x80131BB0 | A01 | `LitModelEmitter` gt3 / gt4 | scenery walker 0x80132358; 0x80132DC0 via 0x8003F698 |
| 0x80130838 / 0x80130D9C, 0x8012F8D8 / 0x8013000C | A01 | `SwayModelEmitter` sway / scroll, cue pair | scenery walker 0x80132358 |
| 0x80132690 / 0x801329C4 | A01 | `UnlitModelEmitter` gt3 / gt4 (`kA01Cue`) | 0x80132DC0 via 0x8003F698 |
| 0x8013544C / 0x8013590C | A05 | `LitModelEmitter` gt3 / gt4 | 0x801362CC via 0x8003F698 |
| 0x8013F418 / 0x8013F6E4 | A04 | `FogModelEmitter` gt3 / gt4 (`kA04`) | not traced |
| 0x8013AC90 / 0x8013AF0C | A05 | `FogModelEmitter` gt3 / gt4 (`kA05`: fog bias 0x40, no hide bits) | scenery walker, branch 0x8013623C |
| 0x8013C0D8 / 0x8013C5B4 | A06 | `LitModelEmitter` gt3 / gt4 (`kA06Flags`) | scenery walker 0x8013CD34; 0x8013D568 via 0x8003F698 |
| 0x8013BA44 / 0x8013BD40 | A06 | `UnlitModelEmitter` gt3 / gt4 (`kA06`) | scenery walker 0x8013CD34 |
| 0x8013CF00 / 0x8013D1E4 | A06 | `UnlitModelEmitter` gt3 / gt4 (`kA06Scroll`) | 0x8013D568 via 0x8003F698 |
| 0x8012CDF4 / 0x8012D2B4 | A07 | `LitModelEmitter` gt3 / gt4 | scenery walker 0x8012DA14; drawer 0x8012E1A0 |
| 0x801311D0 / 0x801313A0 | A07 | `OverlayGt3Gt4` gt3 / gt4 (A00's body) | drawer 0x8012E1A0 |
| 0x80112DEC / 0x80112FBC | A0L | `OverlayGt3Gt4` gt3 / gt4 (A00's body) | 0x8010B1B8 (A00's 0x80146478) |
| 0x801246A4 / 0x8012496C, 0x8012C7E0 / 0x8012CAA8 | A02, A07 | `OverlayGroundGt3Gt4` gt3 / gt4 (`kFarthest`) | scenery walkers 0x80124CB8, 0x8012DA14 |
| 0x8010AA4C / 0x8010AD40 | A0L | `OverlayGroundGt3Gt4` gt3 / gt4 (`kA0L`) | scenery walker 0x8010B0B8 |
| 0x80129BAC / 0x8012A06C | A08 | `LitModelEmitter` gt3 / gt4 | scenery walker 0x8012A7CC |
| 0x80140FBC / 0x801411D8 | A08 | `OverlayGt3Gt4` gt3 / gt4 (A00's body plus a UV scroll) | scenery walker 0x8012A7CC |
| 0x801465EC / 0x801467BC | A00 | `OverlayGt3Gt4` gt3 / gt4 | 0x80146478 via 0x8003F698 |
| 0x8013FB88 / 0x8013FE58 | A00 | `OverlayGroundGt3Gt4` gt3 / gt4 | `entityLoop` |
| 0x801099B4 / 0x80109C80 | SOP | `UnlitModelEmitter` gt3 / gt4 | `SopGround::draw` |
| 0x80112A24 / 0x80112CF0 | A0B | `UnlitModelEmitter` gt3 / gt4 (SOP's body) | not traced |
| 0x80113788 / 0x80113A54 | A0C | `UnlitModelEmitter` gt3 / gt4 (SOP's body) | not traced |
| 0x801103F4 / 0x80110698 and the same body in A0B-A0F, A0H-A0J | A0A-A0J | `UnlitModelEmitter` gt3 / gt4 (`kFlagged`) | not traced |
| 0x80124DB8 / 0x80125100 | A02 | `UnlitModelEmitter` gt3 / gt4 (`kA02`: the resident body, flagged depth from the SZ registers, a three-word U scroll) | drawer 0x8012555C via 0x8003F708 |
| 0x8010F97C / 0x8010FD74 | A0A | `UnlitModelEmitter` gt3 / gt4 (`kA0a`: depth from the SZ registers, byte UV scroll on flag bits 4 and 8) | not traced |
| 0x80112DE4 / 0x801131B0 | A0D | `UnlitModelEmitter` gt3 / gt4 (`kA0d`: kA0a's body with a flag-4 screen wave of rsin(phase + x*4) >> 10) | drawer 0x80113628 |
| 0x80110094 | A03 | `UnlitModelEmitter` gt3 (`kA03`: SOP's projection, always averaged, its band picked from depth - 0xE9 (`kA03BandBias`), two-sided on flag bit 0); its GT4 is `SubdividedGround` | drawer 0x801109D4 |
| 0x80114A5C / 0x80114E70 | A0F | `UnlitModelEmitter` gt3 / gt4 (`kA0f`: kA0a with the first matching scroll only, each with its own bucket) | not traced |
| 0x80113AEC / 0x80113DE0 | A0E | `UnlitModelEmitter` gt3 / gt4 (`kA0eArena`: SOP's body plus the west-record hide by model x while 0x1F80009C is set) | arena walker 0x80114320 |
| 0x8010BC40 / 0x8010BF28 (A0G), 0x8010B3DC / 0x8010B6C4 (A0I), 0x8010AF58 / 0x8010B240 (A0H), 0x8010A3AC / 0x8010A69C (A0J) | A0G-A0J | `UnlitModelEmitter` gt3 / gt4 (`kSopNear`, `kSopNearWide`, `kSopNearLifted`) | not traced |

The scenery walkers (A01, A02, A05-A08, A0L) are guest code.

Still guest code, by census rank: SOP effect 0x8010BF54, A01 0x80132DC0
and the GT3/GT4 emitter copies of the overlays not listed above
whose bodies differ from the natives: A04 0x801175FC/0x801178A0 and 0x8013D0F8/0x8013D568, A05
0x8013484C/0x80134B74, A07 0x8012DBB4/0x8012DE60, A0G 0x8010C3A4/0x8010C664, A0K
0x80114F1C/0x8011562C and 0x801163E0/0x80116708.

Each per-object drawer (A01 0x80132DC0, A05 0x801362CC, A06 0x8013D568, A08 0x8012A9DC) calls one GT3/GT4
pair per branch, so one sub-part never carries two lists of a kind (issue 0030).
