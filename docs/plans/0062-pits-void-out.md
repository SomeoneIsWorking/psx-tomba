# 0062 — Instant-death pits void out (optional mod)

User, 2026-11-12: falling into water voids out (Tomba is put back on safe ground and loses vitality), but
some pits are an instant game over. Make those pits void out too.

## Steps

1. **RE the water void-out.** From Tomba's state machine, find the routine that handles a water fall: the
   trigger (collision/terrain type or kill plane), the vitality loss, where the safe position is saved and
   restored, and the fade/camera reset it runs. Record addresses in `guest-addresses.md`.
2. **RE the pit death.** Find the trigger that sends a pit fall to the death/game-over path and list the
   areas and pits that use it. Note whether a pit kills by a terrain type, a Y kill plane or a per-area
   script, and whether the safe position is still valid there.
3. **Mod.** Under the setting, the pit trigger takes the water void-out path through the game's own routine
   (the same vitality loss, restore and fade), not a host-written position. If a pit's safe position can be
   a spot that leads straight back into the pit, the mod uses the last position the game itself saved on
   solid ground; that rule comes from the RE, not a margin.
4. **Setting.** A saved setting in the config owner, default off, off under oracle comparison. Off is the
   retail path unchanged.

## Proof

For each listed pit, reached through `warp`: with the setting on, falling in voids out to solid ground with
the water fall's vitality loss and fade; with it off, the original game over. A pit fall at zero
vitality after the loss still ends the game the way a water fall at zero does.

## State (2026-11-13)

Step 1 started, nothing built. Vitality is the halfword `G+0x16E` (`0x800E7FEE`, G = `0x800E7E80`), its shown
target `G+0x170`, its maximum the byte `0x800BF87D`. Static writers (`decomp_pipeline.py --refs 0x800E7FEE`,
`scratch/m0062/decomp`) are all gains or resets: `FUN_80049E54` heal by `a1`, `FUN_80049F80` vitality-up,
`FUN_8004A3D4` item pickup (case `0x90` refills), `FUN_80034CE0` item use, `FUN_800783DC` Tomba's spawn (vitality,
spawn position from `0x800A54A8[0x800BF870][0x800BF871]` or the saved `0x800BF890/94/98` when `0x1F800134` is set).
No static writer loses vitality, so the damage path stores through a G register the analyzer does not resolve.
Next: a store watch on `0x800E7FEE` (`PSXPORT_CW`) through a water fall reached by a title warp that places Tomba
over water, to name the damage routine and the void-out's restore, then the same for a pit.
