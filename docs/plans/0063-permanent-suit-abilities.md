# 0063 — Permanent suit abilities (optional mod)

User, 2026-11-12: once a suit is obtained its ability stays. With the swimming suit owned, Tomba swims
without wearing it; with the cold-protection suit (the user recalls a squirrel suit) owned, cold areas no
longer need it worn.

## Steps

1. **RE the suits.** Find the worn-suit state, each suit's item id in the inventory (`0x800BFAB4`, one count
   byte per id), and every site that tests the worn suit to gate an ability: swimming/diving in water,
   cold damage or the cold block, and any other suit-gated ability. Name which suit protects from cold from
   the item table, not from memory. Record the addresses in the decomp symbol map (`external/tomba2-decomp/config/symbols/`).
2. **Mod.** Under the setting, each ability gate asks "worn, or owned" instead of "worn", at the gate's own
   test. The worn suit, its look and its other abilities are unchanged, so wearing another suit keeps that
   suit's own powers. Inventory and save data are never written.
3. **Setting.** A saved setting in the config owner, default off, off under oracle comparison. Off is the
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
cold immunity under the mod, matching the user's report that the Squirrel suit protects from cold.

Built (2026-11-13): `PSXPORT_TOMBA2_PERMANENT_SUITS` (`ModSettings::permanentSuits`) and `SuitAbilities`
(`game/player/suit_abilities.h`), which owns the item-to-bit table and answers "worn, or owned under the mod" for
suit ids 0x0E..0x16 (Clear Fruit's 0x200 stays worn-only). Owned gates:
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

Left worn-only by design: the 0x20 sites `0x8003B588` and `0x800597AC` copy `G+9` to `G+8` with the Bird suit, the
worn suit's look, which step 2 keeps. Open design choice: the robes' 0x7C00 magic (`0x800552EC`) indexes its cost table
`0x800A4550` by the worn suit's low nibble, so with several robes owned the mod has no single robe to pick; it stays
worn-only until that is decided (other low-nibble readers: `0x8005800C`, `0x8005F778`, `0x80061B58`, `0x80062630`).

Every native gate logs its decision on the `suits` debug channel (`TombaStep::has`).

Still on the suit word, by the scan of every `lh/lhu 0x17E` + `andi` (MAIN and the area overlays):
- MAIN 0x30 `0x800532A0`, `0x80055FBC`; 0x60 `0x80055FBC`, `0x80062A30` (no
  code reference); 0x7C00 `0x800552EC`;
- overlays: 0x100 in A06 `0x8010B410`, A08 six sites, A0E
  `0x80109790`; 0x40 in A01 `0x8010D9FC`, A04 `0x80113414`, `0x80115F6C`, A0B `0x8010A9C8`; 0x20 A01 `0x8010C8CC`;
  0x640 A05 three sites; 0x30, 0x60, 0x180, 0x400, 0x7C00 in A04, A07, A08, A0B.
