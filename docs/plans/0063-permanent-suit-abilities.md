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

Next: list every gate site per bit with its enclosing function and own each test; read the A04 powder sites.
