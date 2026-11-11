# Tomba! 2 producers

Which guest draw functions key their packets for 60 fps interpolation, what each saves per frame, and
how the composer renders it between two frames. The mechanism is psxport `presentation.md`
(Interpolation, Frame model); the owners are in `docs/codemap.md`.

Gameplay in-betweens are not composed from these: every call of the field or room draw is replayed (`DrawReplay`,
`docs/plans/replay-interpolation.md`), paused frames and the DEMO attract demo included. A producer's state render runs
only for a frame whose draw did not run the section: the title menu and the new game's scripted opening (SOP in GAME,
then OPN). The SOP intro at boot presents once per frame and composes nothing. A producer drawn only in gameplay
(every area overlay's) has no state render and saves nothing (issue 0055). The keys stay; they name the packets'
objects for the depth test.

## Keyed producers

A producer is a native override whose packet stores carry the key `(producer, object, element, part)`
(psxport `presentation.md`, Interpolation). The object is the drawn thing: a render command, a
sub-part, a scenery table slot or a map cell. Model emitters name each primitive as an element of that
object, `modelElement(list, index)` (`model_element.h`): GT3 or GT4 list and the index in it, so a
culled primitive does not shift its neighbours and two commands drawing one shared model
(`*(0x800ECF58 + type*4) + modelOffset`) stay apart. Declared through
`declareOverride`/`declareOverlayOverride`'s `producer` argument.

| Guest address | Image | Owner | Arg | Object |
|---|---|---|---|---|
| 0x8003CDD8 | MAIN | `Render::cmdListDispatch` | A0 | per render command (`node+0xC0+4i`); opened in the body, since `PerObjBillboard` calls it directly |
| 0x8003F174 | MAIN | `Render::subPartWalk` | A0 | per sub-part |
| 0x8003F07C | MAIN | `Render::sharedTransformWalk` | A0 | per sub-part |
| 0x801401B8 | A00 | `OverlayGroundGt3Gt4::entityLoop` | A0 | per scenery table slot |
| 0x80115598 | A00 | `TileGridLayer::emit` | A0 | the grid node (every tile sprite and the draw mode packet are one object) |
| 0x8010C26C | SOP | `TileGridLayer::emitSop` | A0 | the grid node |
| 0x801142EC, 0x801141B0, 0x80116B9C, 0x80116778 | A0A, A0B, A0D, A0F | `TileGridLayer::emitUnbiased` | A0 | the grid node |
| 0x80109FE0 | SOP | `SopGround::draw` | A0 | per ground block (`blocks + index*4`; the visible list is rebuilt each frame, the block is not) |
| 0x80078CA8 | MAIN | `Font::glyphEmit` | A3 | per glyph: its character's address (the string's own scope keeps the icon glyphs' packets) |
| 0x80116904 | A08 | `RainStreaks::draw` | A0 | per drop: its trail slot `0x801485E8 + 4i` (fixed seed, so drop i is one lattice point) |
| 0x80141A94 | A06 | `TileGridLayer::emitClamped` | A0 | the grid node |

## State producers

A state producer saves what it drew at each frame (`core.frameStates.save` under the object's scope) and has a
`render(from, to, t)` the composer runs for the object between two frames (psxport `presentation.md`, Frame
model). Each owner's `registerStateRenders` installs it in `tomba::Session::stateProducers` from `tomba::Runtime::registerOverrides`, keyed by the producer address.
Tomba's frame cuts stay `tomba::FrameCut`.

| Producer | Object | Saved state | Render |
|---|---|---|---|
| 0x80110290 `SubdividedGround` (A03; gameplay only, its removal is pending with its owner, issue 0055) | the emitter call | a `ListJob`: the records and the two split counters 0x8011C0E0/E4 | the body over host memory, both subdivisions included, cut at the draw window |
| 0x80078CA8 `Font::glyphEmit` | one glyph, by character address | `AnchoredPackets` (`render/anchored_packets.h`): the OT slot, the anchor, each packet's bucket and command words | the anchor moves by t and every vertex with it; the string's texture page is the constant glyphEmit passes to SetDrawMode |
| 0x8007E9C8 `ScreenFade::draw` | one call: its call site, n-th in the logic frame (`CallSiteObjects`, `render/call_site_objects.h`, held by `tomba::Session::callSites`) | the OT slot and bucket, colour, span and blend page | the colour moves by t when the blend is unchanged |
| 0x8007E1B8 `UiSprite::quads`, 0x8007E6DC `UiSprite::compose` | one call, by call site as above (the DEMO menu 0x80106824 and every 2D sprite reach these) | `AnchoredPackets` from the placement word; compose adds the definition's texture page | the anchor moves by t |
| 0x8003C8F4 `Render::billboardEmit` (reached from 0x8003C2D4 and the other three composers) | the call: the n-th call from the site drawing this node (`CallSiteObjects::object(returnAddress, node)`; a composer reaches one node twice a frame, so the node alone would be two serials and ambiguous) | `BillboardState`: GTE CR0-7 and per drawn particle its corners, bucket and packet | the transform and corners move by t, then RTPT/RTPS project again; the packet words (colour, CLUT, UV) are the later state's, so a flipbook frame (A00's flame steps U by 0x20 a frame) changes on the in-between |
| 0x8007CC00 `DialogGlyphs` | the dialog box's glyphs, by the box | `AnchoredPackets`: the box's rect, each glyph packet's bucket and command words | the box's rect moves by t and every glyph with it |
| 0x8007CDD4 `DialogGlyphs::prompt` | the dialog box | `AnchoredPackets`: the box's rect, the prompt sprite and its mode packet in bucket 2 | the box's rect moves by t and the prompt with it |
| 0x8005019C `Panel::build`, 0x8004FFB4 `Panel::fillQuad` | one call, by call site as the 2D writers | `AnchoredPackets` from the panel's rect | the rect moves by t |
| 0x80129114, 0x8012D9E8, 0x8013E08C `GuestKeyedEmitters` A01/A00 leaves, MAIN 0x800328BC (a node's 0x80027A4C stamp, called from the area effect leaves), A07 0x80117D4C (strip through 0x80117AB8), A0D 0x80110C14 (ring of stamps), A0F 0x80116ABC (wire of 0x80112D10 lines) and 0x801143C4 (lens flare and its 0x80027A4C sparkles); A03 0x801113B4 (ribbon of 0x80110B00 quads), A06 0x80129AF8 (line, keyed by call site); 0x801311FC `::a01Effect`, A03 0x80109EEC (HUD) | the node in a0 for a leaf, one screen for a body that takes no object (`quad_state.h`, `runGuestEmitter`) | the packets the guest body linked (`quad_state.h`) | the packets' screen vertices move by t |
| 0x80026864 `LetterboxBars::tick` | one call, by its slot | the two bars' height and the OT slot (`Saved`) | the bar height moves by t and both rects are re-emitted |

Not state producers: `SopGround::draw` 0x80109FE0, `Render::cmdListDispatch` 0x8003CDD8, `Render::subPartWalk`
0x8003F174, `Render::sharedTransformWalk` 0x8003F07C and `OverlayGroundGt3Gt4::entityLoop` 0x801401B8 open
scopes only; their packets come from the emitters above. A packet a scope's guest code writes itself stays as the
guest drew it. Keyed blend still runs for what is left keyed (psxport `FramePresenter::presentRecords`); see
issue 0033.

Element namers (not producers; they name elements of the innermost open object):

| Guest address | Image | Owner | Reached from |
|---|---|---|---|
| 0x8007FDB0 / 0x8008007C | MAIN | `UnlitModelEmitter` gt3 / gt4 | 0x800803DC via 0x8003F698 |
| 0x801316A8 / 0x80131BB0 | A01 | `LitModelEmitter` gt3 / gt4 | scenery walker 0x80132358; 0x80132DC0 via 0x8003F698 |
| 0x80130838 / 0x80130D9C, 0x8012F8D8 / 0x8013000C | A01 | `SwayModelEmitter` sway / scroll, cue pair | scenery walker 0x80132358 |
| 0x80132690 / 0x801329C4 | A01 | `UnlitModelEmitter` gt3 / gt4 (`kA01Cue`) | 0x80132DC0 via 0x8003F698 |
| 0x8013544C / 0x8013590C | A05 | `LitModelEmitter` gt3 / gt4 | 0x801362CC via 0x8003F698 |
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

The scenery walkers (A01, A02, A05-A08, A0L) are guest code and open no scope; there an emitter call is its
own object (producer = the emitter, object = its record list, which is the table slot's).

An object keyed by a render record (`cmdListDispatch`, both sub-part walks) is the record's current life,
`RenderRecordPool::object(record)`: despawn pushes the record back on the free stack and the next spawn
pops it, so the next object on it gets another key.

Not yet producers, by census rank: SOP effect 0x8010BF54, A01 0x80132DC0
and the GT3/GT4 emitter copies of the overlays not listed above
whose bodies differ from the natives: A03 0x80110094/0x80110290, A04
0x801175FC/0x801178A0 and 0x8013D0F8/0x8013D568, A05 0x8013484C/0x80134B74 and 0x8013AC90/0x8013AF0C, A07
0x8012DBB4/0x8012DE60, A0D 0x80112DE4/0x801131B0, A0G 0x8010C3A4/0x8010C664, A0K 0x80114F1C/0x8011562C and 0x801163E0/0x80116708
(issue 0028).

Each per-object drawer (A01 0x80132DC0, A05 0x801362CC, A06 0x8013D568, A08 0x8012A9DC) calls one GT3/GT4
pair per branch, so one sub-part never carries two lists of a kind (issue 0030).
