# 0063 — Permanent suit abilities (optional mod)

User, 2026-11-12: once a suit is obtained its ability stays. With the swimming suit owned, Tomba swims
without wearing it; with the cold-protection suit (the user recalls a squirrel suit) owned, cold areas no
longer need it worn.

User, 2026-11-13: "I think we should be able to toggle suit effects for owned suits, that's the simplest, and if a
suit has more than one effect then all of them should be toggle-able individually".

## Steps

1. **RE the suits.** Find the worn-suit state, each suit's item id in the inventory (`0x800BFAB4`, one count
   byte per id), and every site that tests the worn suit to gate an ability: swimming/diving in water,
   cold damage or the cold block, and any other suit-gated ability. Name which suit protects from cold from
   the item table, not from memory. Record the addresses in the decomp symbol map (`external/tomba2-decomp/config/symbols/`).
2. **Mod.** Each ability gate asks for its own ability (`SuitAbility`), not the suit bit: the worn suit passes as
   in retail, an owned one only for the effect its setting turns on. The worn suit, its look and its other
   abilities are unchanged. Inventory and save data are never written.
3. **Settings.** One saved setting per owned-suit effect in the config owner (`ModSettings::suitEffect`,
   `PSXPORT_TOMBA2_SUIT_*`, a settings-menu row each), default off, off under oracle comparison. All off is the
   retail path unchanged.

## Proof

Through `warp` and a card with the suits owned: with the setting on and a different suit worn, Tomba swims in
deep water and takes no cold effect in a cold area; with it off, both behave as retail. A save made with the
setting on loads identically with it off.

## State (2026-11-13)

Static only. The worn suit is Tomba's u16 `G+0x17E` (`0x800E7FFE`), saved as `0x800BF89E` and copied back by the
spawn `0x800783DC`; bit 0x8000 is the grown form, 0x200 a state the item uses refuse in. Tests of the word by
mask (MAIN, `lh/lhu 0x17E` then `andi`): 0x100 once, the swim branch of the water landing `Tomba_waterLanding`
(`0x800665F4`) and of the area ticks (A00 `TombaTick_hazardFloor` `0x8010E408`: water floor without 0x100 is
the water fall); 0x40 seven times (`0x80056CE8`, `0x800570A8`, `0x80057170`, `0x80057334`, `0x8005B780`,
`0x8005C354`, `0x8005D358`; the community Squirrel Suit flight codes patch `0x8005709A`/`0x8005721C` beside
the second); 0x20 at `0x8003B6BC`, `0x800597E4`; 0x30, 0x60, 0x180, 0x640, 0x7C00 composites. Equipping is not
in item use `0x80034CE0`.

The word is derived from the worn item `G+0x6F` by `Tomba_suitWord` (`0x80067EF4`, decomp symbol map), which also
stores `0x800BF89E`: the low nibble is the suit's index and one ability bit per suit; grown (0x8000) is kept for
none and 0x15 and forced by `G+0x174 & 4`. Worn item → word: none 0x10, 0x0E 0x22, 0x0F 0x41, 0x10 0x83,
0x11 0x10A, 0x12 0x404, 0x13 0x805, 0x14 0x1006, 0x15 0x2007, 0x16 0x4008, 0x6C 0x209. So swimming (0x100) is
item 0x11 and the 0x40 gates (beside the Squirrel flight codes) are item 0x0F. `Tomba_suitEvent` (`0x80058304`,
cases 6/7 grow and shrink, 0xC the reset that turns 0x200 back to none) is the other writer. The mod's rule
follows: a gate testing bit b of `G+0x17E` also passes when the inventory (`0x800BFAB4`) owns the item whose word
carries b, read from this table, never written into the word (it is saved).

Item names: `g_ItemText` (`0x800A2BE8`, 29 code sites) holds 12-byte {kind, name, description} entries per item id:
0x0E Bird Clothes, 0x0F Squirrel Clothes, 0x10 Pig Suit, 0x11 Swimming Pig Suit, 0x12..0x16 the Evil Flame, Ice,
Ghost, Earth and Water Pig Robes, matching the bits above (0x0F carries 0x40, 0x11 the swim bit 0x100).

Cold: the idle state `ActorTomba_idleColdGate` (`0x8005A910`, native `ActorTomba::idleColdGate`) runs the A04
cold idle `TombaIdle_cold` (`0x80112B50`) instead of the normal idle `0x8005A970` when the cold ground flag
`G+0x17C` is set (only A04 code writes it) and `G+0x17E & 0x640` is clear. Standing still there, Tomba shivers
at 0x5A frames and is frozen at 0x6E, losing vitality unless Cold Powder (`0x800BFAD5`) is held. Bits 0x40
(Squirrel Clothes), 0x200 (item 0x6C) and 0x400 (Evil Flame Pig Robe) skip it, so owning any of those grants
cold immunity under their settings, matching the user's report that the Squirrel suit protects from cold.

Built (2026-11-13): `SuitAbilities` (`game/player/suit_abilities.h`) maps each guest gate (its mask) to the owned
suits and effects that may pass it; Clear Fruit's 0x200 stays worn-only. The settings, one per effect:

| setting `PSXPORT_TOMBA2_SUIT_` | suit | ability (mask) |
|---|---|---|
| `BIRD_FLUTTER` | Bird Clothes 0x0E | fall flutter (0x20) |
| `BIRD_AIR_CONTROL`, `SQUIRREL_AIR_CONTROL` | Bird, Squirrel Clothes 0x0F | winged rise and air steer (0x60) |
| `SQUIRREL_GLIDE` | Squirrel | glide (0x40) |
| `SQUIRREL_COLD`, `FLAME_ROBE_COLD` | Squirrel, Evil Flame Pig Robe 0x12 | cold immunity (0x640) |
| `PIG_JUMP`, `SWIMMING_PIG_JUMP` | Pig Suit 0x10, Swimming Pig Suit 0x11 | launch, rise and release brake (0x180) |
| `PIG_STOMP`, `SWIMMING_PIG_STOMP` | Pig, Swimming Pig | air stomp (0x180) |
| `SWIMMING_PIG_SWIM` | Swimming Pig | swim (0x100) |
| `SNOW_GRIP` | any owned suit but the Bird | lifts the snow slide (0x30 handicap) |
| `FREEZE_GUARD` | any owned suit but the Bird | area 4's ice monster does not freeze him (0x30 handicap) |
| `SQUIRREL_UPDRAFT` | Squirrel | longer reach into updrafts (0x40) |
| `FLAME_ROBE_FREEZE` | Evil Flame Pig Robe | the ice pig battle's updraft does not freeze him (0x400) |

Robe magic has no settings yet. `0x800552EC` lets a robe cast (button 0x40) while the worn word has a 0x7C00 bit and
magic `0x800BF87E` covers `0x800A4550[word & 0xF] >> 1`; the cast then follows the worn word's low nibble
(`0x8005800C`, `0x8005F778`, `0x80061B58`, `0x80062630`). The game keeps only the current word (saved `0x800BF89E`),
no history of robes worn, so with several robes owned and none worn nothing in it picks one. A robe's setting is
faithful only while it is the one robe effect on; that rule and the four nibble readers come with the robe gates.

Per-effect independence: with the Pig Suit owned and not worn in area 1, `PIG_JUMP` on and `PIG_STOMP` off jumps
to Y 0xE627 and a second jump press in the air does nothing; `PIG_JUMP` off and `PIG_STOMP` on jumps to the retail
0xE664 and the press stomps (mode 0x1C) (`scratch/m0063/pig_jumpOnly`, `pig_stompOnly`).

Owned gates (the proofs below predate the per-effect settings, each run turning on the effects of the suit it
owns):
- the idle cold gate (`ActorTomba::idleColdGate`, 0x640): with the Squirrel Clothes owned and not worn (word 0x10),
  Tomba stands in area 4 without freezing for 480 frames; off, he freezes and loses a point (`scratch/m0063/own_on`,
  `own_off`);
- the airborne step FUN_800574E0 (`Airborne::step`, with 0x8005706C, 0x800572EC and 0x80057150 inside it): every
  0x20/0x40/0x60/0x180 test. A held jump in area 1 with the Squirrel owned glides and lands about 25 frames later
  than with the mod off (`scratch/m0063/glide1_sheet.png`, `glide0_sheet.png`).

- the ledge drop FUN_80056C00 (`Airborne::drop`, 0x40): walking off area 1 entry 1's ledge with the Squirrel owned
  quarters the fall's start and glides onto the ledge below (Y 0xE89E at f165); off, he drops to 0xEA34
  (`scratch/m0063/drop1`, `drop0`);
- mode 6, the air attack FUN_8005D16C (`ActionModes::mode6`, 0x40): with weapon 1 or 2 (`G+0x6C` from `0x800BF88C`)
  and circle held, the Squirrel arms the glide (`G+0x178 = 0xC`); a jump plus held circle in area 1 lands at f181
  instead of f167 (`m6_1`, `m6_0`);
- modes 3 and 0x10 (`ActionModes::mode3`, `mode16`, FUN_8005B63C, FUN_8005C26C, 0x40): native; a grab of the
  enemy at area 1 sector 10 runs mode 3 substates 8, 9, 3, 4, 5, 7 (`grab10x`). Their 0x40 site is substate 1, entered
  only for a target whose kind (`0x8005B5E4`) is 0; no such target is found yet, so that gate is unproven;
- swimming (`WaterEntry`, 0x100): A00's floor tick FUN_8010E408 and the water landing FUN_800665F4. Spotted over area 0
  water (sector 22, 14700 -400 10600) with the Swimming Pig Suit owned and not worn, Tomba swims (mode 0x1D); off,
  the water fall (`swim1`, `swim0`, `swim_sheet.png`). The landing (callers `Tomba_hazardHit` 0x8006670C,
  0x80066DE8: a hit onto water) is not reached yet.
- the Pig Suit's 0x180 (`Airborne::launch` FUN_80055F48, the jump release FUN_80056E08 inside `Airborne::step`,
  `Airborne::airActions` FUN_80057C08): with the Pig Suit owned and not worn, a held jump in area 1 starts 0x1940
  faster and peaks at Y 0xE627 instead of 0xE664, and a second jump press in the air stomps (mode 0x1C, pose 0xE5);
  off, neither (`pig1`, `pig0`).
- the x speed step FUN_80055FBC (`RunSpeed::step`): the winged suits' (0x60) air steer, the table step times 16
  instead of 2: with the Bird owned, a mid-air reversal in area 1 steps 0x600 a frame and reaches -0x1B00 before
  landing; off, 0xC0 a frame (`steer1`, `steer0`). Its 0x30 test is a handicap, not an ability: with no suit (word
  0x10) or the Bird, snow (floor type 0x500, areas 4 and 5) cuts the start to an eighth and the brake to half, so
  `SNOW_GRIP` lets any owned suit without 0x30 lift it (`SuitAbilities::slipsOnSnow`). With the Swimming Pig Suit owned
  and not worn, the landing slide on area 4's snow brakes 0x280 a frame and stops in 11; off, 0x140 and 22
  (`snow1`, `snow0`). With the mod off the native's G+0x44 stores match the guest's one for one on ground turns,
  skids and the snow slide (`vx_g5`/`vx_n5`, `vx_g6`/`vx_n6`).

`0x800532A0` keeps its 0x30 test in guest code: it sends those suits to mode 0x35 on a type 0x500 floor with bits
12..15 = 4, and no area's grid has one.

- area 5's cold field (`ColdField`, A05 FUN_8010A854 the area tick and FUN_8012E454 the ice crust, 0x640): with the
  Squirrel owned and `SQUIRREL_COLD` on, Tomba walks right from area 5 entry 0 to Z 0x22C5 untouched, as with the
  Squirrel worn; off, or with no suit, he freezes and loses a point at f227, f347 and f443 and the game-over substate
  starts at f547, the native's frames identical to the guest's (`cold_n_on`, `cold_n_off`, `cold_n_none`,
  `cold_g_none`, `cold_g_worn`, `cold_sheet.png`).

- area 6's floor tick (`WaterEntry::area6Tick`, A06 FUN_8010B334, 0x100; it swims only with the area flag
  `0x800BFE56 & 0x40`): spotted over area 6 water (`spot 6 1 2 23168 -1600 14912`, flag set) with the Swimming Pig
  owned and `SWIMMING_PIG_SWIM` on, Tomba swims (mode 0x1D); off, the water fall and a point, as the guest
  (`sw6_on`, `sw6_off`, `sw6_g`, `sw6_sheet.png`).

- areas 8 and 14's floor ticks (`WaterEntry::area8Tick` A08 FUN_8010B328, `WaterEntry::area14Tick` A0E
  FUN_8010967C, 0x100): spotted over water (`spot 8 1 1 8064 -3900 7808`; `spot 14 0 5 10240 -3070 11840` with
  `0x800BFA1A & 0x10` set to pass the Water Pig Bag scene) with the Swimming Pig owned and `SWIMMING_PIG_SWIM` on,
  Tomba swims at f18 with his 4 points, as with the suit worn; off or with no suit, the water fall and a point, frame
  for frame the guest's (`w_n_a8*`, `w_n_a14*`, `w_g_a8*`, `w_g_a14*`).

- area 8's waterfall (`WaterEntry::waterfallContact`, A08 FUN_8010D334, 0x100): spotted under the pool's fall
  (`spot 8 9 6 15221 -6400 4160`) with the Swimming Pig owned and `SWIMMING_PIG_SWIM` on, Tomba stays in the pool in
  mode 0x10 as with the suit worn; off or with no suit he is knocked out (hazard substate 2) at f66, the guest's
  timeline (`w_cn_a8*`, `w_c_a8*`, `fall8_sheet.png`). Under `PSXPORT_OVERRIDE_DIFF` the off runs differ from the
  guest only in v0/v1, which these void functions leave as scratch (0 memory bytes).

Left worn-only by design: the 0x20 sites `0x8003B588` and `0x800597AC` copy `G+9` to `G+8` with the Bird suit, the
worn suit's look, which step 2 keeps. Open design choice: the robes' 0x7C00 magic (`0x800552EC`) indexes its cost table
`0x800A4550` by the worn suit's low nibble, so with several robes owned the mod has no single robe to pick; it stays
worn-only until that is decided (other low-nibble readers: `0x8005800C`, `0x8005F778`, `0x80061B58`, `0x80062630`).

Every native gate logs its decision on the `suits` debug channel (`TombaStep::has`).

Still on the suit word, by the scan of every `lh/lhu 0x17E` + `andi` (MAIN and the area overlays):
- MAIN 0x60 `0x80062A30` (the climb step, below); 0x7C00 `0x800552EC`;
- area 4's ice monster (`FreezeTouch::contacts`, A04 FUN_8010F1CC, the contact loop; kind 0x28 tests 0x30): in
  light clothes its touch freezes Tomba (mode 0 substate 4). Its own effect, `FREEZE_GUARD`, beside `SNOW_GRIP`,
  both through `SuitAbilities::lightlyDressed`. From `spot 4 1 23 3392 -10500 10600` with the Squirrel owned and the
  setting on, the touch knocks Tomba back (mode 0x34) as with the Squirrel worn; off or with no suit he freezes at
  f66, and under `PSXPORT_OVERRIDE_DIFF` 200 of 200 calls match the guest (`f4n_*`, `f4g_*`, `freeze4_sheet.png`).
  `0x80110A30` repeats case 0x28 and, like `0x80111398` and `0x80115EF0`, has no reference in any overlay, MAIN or
  area 4's RAM, so those stay guest code. `0x8010F75C`'s kind 1 test needs `G+0x16A == 8`, set only on a floor no
  area has, so it stays guest code.

- area 4's floor tick (`SnowFieldTick::tick`, A04 FUN_8010B024, 0x60 asked as AirControl): off the slope, in
  collision sector 0x27 or above, a winged suit arms the 4-frame angle-wrap countdown `0x800BF819`. From
  `spot 4 1 43 4736 -7200 8000` with the Squirrel owned and `SQUIRREL_AIR_CONTROL` on it is armed, as with the
  Squirrel worn; off or with no suit it stays 0, and 152 of 152 differential calls match the guest (`b819n_*`,
  `b819g_*`).
- area 1's steam vents (`UpdraftContact::steamVent`, A01 FUN_8010D90C, 0x40 asked as `Updraft`): the Squirrel grows a
  vent's lift box by 400 instead of 200, so it floats higher. New effect `SQUIRREL_UPDRAFT`. Standing on vent 0
  (`spot 1 1 2 10496 -7700 9040`, `0x800BF9F4 = 0xFF`) with the Squirrel owned and the setting on, Tomba floats at
  y -9028..-8896, the same as with the Squirrel worn; off or with no suit he floats at -8828..-8696. The
  differential matched the guest on 1782 of 1782 calls, both worn and off (`s_vent_*`, `s_dvent_*`).
- area 1's floor tick (`PipeAreaTick::tick`, A01 FUN_8010C7BC, 0x20 asked as `Flutter`): a fluttering Bird in
  x 0x28B5..0x2D69 below y -0x1A8F sinks 0x20 a frame. With the Bird owned and `BIRD_FLUTTER` on, from
  `spot 1 1 3 11000 -6300 9050` with jump held, the flutter (`G+0x179`) sinks him 0x20 a frame faster than with no
  suit. The differential matched the guest on every call (198 and 108 calls, worn and off: `s_flut_*`, `s_dflut_*`).
- A0B, the Evil Ice Pig battle (`UpdraftContact::area11Updraft`, FUN_8010A93C): the same lift (0x40), and after 60
  frames of touches a freeze unless the Evil Flame Pig Robe is worn (0x400, `FreezeImmunity`). New effect
  `FLAME_ROBE_FREEZE`. The updraft is the boss's whirlwind (kind 0x55, `0x80117FD8`, tick `0x801179BC`), thrown only
  in boss mode 6 (`0x8010F9D8`), which `0x8010D1F0` picks when the boss (kind 0x66, `0x800FB218`) is down to 1
  hit point (`+0x6E`; 2 when `0x800BF870 == 0xF`). From `warp 11/0`, circle through the dialogue to f1140, then the boss
  held at 1 point and Tomba at 4 (`w16 800FB286 1`, `w16 800E7FEE 4` every 30 frames): with no suit the whirlwind
  lifts him and freezes him (hazard mode 6, then mode 0x1A) at f1593; with the Robe owned and `FLAME_ROBE_FREEZE` off
  at f1348; with it on (15 asks granted) and with the Robe worn (67) he is lifted and set down unfrozen. Under
  `PSXPORT_OVERRIDE_DIFF` the off run matched the guest on 381 of 381 calls (`ice_*`, `ice_worn2`). The frost actor `0x80114608` tests 0x400 only while
  `G+0x177 & 2` is set, and only the frozen states set that bit, so it stays guest code.
- the climb step (`ClimbStep::step`, FUN_80062A30, 0x60 asked as `AirControl`): G+0x156 is Tomba's place down the
  climbable (step table `0x800A4700`: -0x10 up, +0x16 down). Climbing down past the end of a moving kind 0x17 rope he
  drops off with its velocity, and a winged suit starts `(velocity & 0x1F00) >> 5` lower. The ropes are A01's
  pulleys (`0x80126608` spawns two linked ropes, `+0x47` 0 and 1, and a still knot, 2; tick `0x8012A454`); a weighted
  rope pays out at up to 0x1F00 (`0x80129C70`), so the drop is 0xF8 lower. From `spot 1 1 2 8500 -6300 9021` with
  down held from f1, the Bird worn and the Bird owned with `BIRD_AIR_CONTROL` on both let go at place 0xD7C; off or
  with no suit at 0xC3D, 0xBF higher at the same frame; then the pit below (`s_ropeb_*`, `s_ropew1_worn`). Under
  `PSXPORT_OVERRIDE_DIFF` the off run matched the guest on 75 of 75 calls and the on run differed only in that one
  call's G+0x156, 0xC84 against 0xD7C (`s_rope_*`, `s_ropebd_off`).
- costume checks stay worn-only. They react to what Tomba is wearing, so an owned suit does not trip them. A07's
  0x180 predicates `0x80130CF8`, `0x80130D44`, `0x80130D80`, `0x80130DD8` choose the Circus Village people's lines
  (`0x80042354`) when Tomba wears a Pig suit. A07 `0x80121C64`/`0x80125BAC` (event steps at `0x80134158`,
  `0x801349F0`) dress him in the Pig Suit when he is not in one, and A08 `0x80132564` and `0x801219A0` (0x2210) take
  the suit off.
- robe magic (0x7C00): A07 `0x8010BC9C` makes its actor a target (`0x1F800184`, `0x800BF840 = 0x4B`) only while a
  robe is worn. It belongs to the robe cast, `0x800552EC`, and stays worn-only along with it (see the open choice
  above).
