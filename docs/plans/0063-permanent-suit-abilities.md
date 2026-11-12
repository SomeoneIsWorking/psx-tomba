# 0063 — Permanent suit abilities (optional mod)

User, 2026-11-12: once a suit is obtained its ability stays. With the swimming suit owned, Tomba swims
without wearing it; with the cold-protection suit (the user recalls a squirrel suit) owned, cold areas no
longer need it worn.

## Steps

1. **RE the suits.** Find the worn-suit state, each suit's item id in the inventory (`0x800BFAB4`, one count
   byte per id), and every site that tests the worn suit to gate an ability: swimming/diving in water,
   cold damage or the cold block, and any other suit-gated ability. Name which suit protects from cold from
   the item table, not from memory. Record the addresses in `guest-addresses.md`.
2. **Mod.** Under the setting, each ability gate asks "worn, or owned" instead of "worn", at the gate's own
   test. The worn suit, its look and its other abilities are unchanged, so wearing another suit keeps that
   suit's own powers. Inventory and save data are never written.
3. **Setting.** A saved setting in the config owner, default off, off under oracle comparison. Off is the
   retail path unchanged.

## Proof

Through `warp` and a card with the suits owned: with the setting on and a different suit worn, Tomba swims in
deep water and takes no cold effect in a cold area; with it off, both behave as retail. A save made with the
setting on loads identically with it off.
