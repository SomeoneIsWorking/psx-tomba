# 0064 — Charles co-op (optional mod)

User, 2026-11-12: a Tomba! 2 mod where a second player plays Charles alongside Tomba.

Rules from the user:
- Local only: pad 2 drops in and out. No online play.
- Charles cannot leave the screen; the screen edge holds him.
- Charles has no HP and cannot lose any. A hit gives him only the physical reaction (knockback, hurt
  animation).
- A fall (pit or water) puts Charles back beside Tomba, with no penalty.

## Steps

1. **RE Charles.** Find Charles's actor (class, record layout, spawn and despawn), what drives him now
   (script, follow AI or cutscene only) and in which areas and events he exists. Find the player actor's
   input read and the per-frame update both share, so a second pad can drive Charles through the same
   movement and action code Tomba uses, not a host-written position.
2. **Design from the RE.** Write here: which of Charles's moves exist in the game's own code; the camera
   stays Tomba's, and Charles is clamped to the visible edge (the widescreen draw window when wide); where
   the game's hit path applies knockback, so Charles takes the reaction with the damage step skipped; the
   fall trigger that returns him beside Tomba, through the game's own reposition; and drop-in/drop-out on
   the second pad's Start.
3. **Mod.** Pad 2 input reaches Charles's controller through one owner; story scripts that move Charles keep
   priority over pad 2. Guest progression state (flags, inventory, events) is never written by the mod.
4. **Setting.** A saved setting in the config owner, default off, off under oracle comparison. Off is the
   retail path unchanged.

## Proof

With two pads, reached through `warp`: pad 2 drops in as Charles, moves and acts with the game's own
animations and drops out. He stops at the screen edge when Tomba walks away, is knocked back by an enemy
with no HP change anywhere, and a pit or water fall puts him beside Tomba. A scripted scene still moves
Charles as retail.
With the setting off, pad 2 does nothing, as retail.
