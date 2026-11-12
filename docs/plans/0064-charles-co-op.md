# 0064 — Charles co-op (optional mod)

User, 2026-11-12: a Tomba! 2 mod where a second player plays Charles alongside Tomba.

## Steps

1. **RE Charles.** Find Charles's actor (class, record layout, spawn and despawn), what drives him now
   (script, follow AI or cutscene only) and in which areas and events he exists. Find the player actor's
   input read and the per-frame update both share, so a second pad can drive Charles through the same
   movement and action code Tomba uses, not a host-written position.
2. **Design from the RE.** Write here: which of Charles's moves exist in the game's own code, how the camera
   keeps both players on screen (the game's camera inputs, widened as the widescreen canvas allows), what
   happens when Charles leaves the screen or falls (warp to Tomba, by the game's own reposition), and how
   drop-in/drop-out works on the second pad's connect and Start.
3. **Mod.** Pad 2 input reaches Charles's controller through one owner; story scripts that move Charles keep
   priority over pad 2. Guest progression state (flags, inventory, events) is never written by the mod.
4. **Setting.** A saved setting in the config owner, default off, off under oracle comparison. Off is the
   retail path unchanged.

## Proof

With two pads, reached through `warp`: pad 2 drops in as Charles, moves and acts with the game's own
animations, stays on screen with Tomba and drops out; a scripted scene still moves Charles as retail.
With the setting off, pad 2 does nothing, as retail.
