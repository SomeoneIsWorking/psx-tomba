# 0061 — Real sea and sky with clouds (optional mod)

User, 2026-11-12: replace the sea/sky background atlas with a real sea and a real sky with clouds, as an
option. Extends G006 (Better water) to the backdrop.

## What is drawn now

Field backdrops are 2D tile grids (`TileGridLayer`, `game/render/tile_grid_layer.cpp`,
`native-render-2d-tilegrid.md`): 16x16 tiles from a VRAM atlas walked over a map, scrolled by the camera,
with CLUT palette cycles for the water shimmer (`palette_cycles.*`). Gradient skies are G4 bands
(`GradientBackdrop`, A0E).

## Steps

1. **RE the backdrop.** For each area with a sea/sky backdrop, name the grid emitter, its map and atlas
   (tpage, CLUT, cell range), its OT bucket, scroll inputs and parallax factor, and the palette cycle that
   animates the water. Record in the decomp (`external/tomba2-decomp`: symbol map and `docs/notes/field-tile-grid.md`); which areas carry
   it goes in this plan.
2. **Identify the sea/sky cells.** Classify map cells as sky, sea or other (shore, islands) from the map
   data, never from pixels. The horizon line comes from the map rows where sky cells end.
3. **Host render.** Under the setting, the grid emitter skips sky and sea cells and the host draws, in the
   same OT bucket: a sky (gradient plus procedural clouds drifting with the guest's scroll and parallax)
   and a sea surface (procedural waves to the horizon, phase from the guest's palette-cycle counter so it
   moves when the original moves). Both read the blended camera, so in-betweens are smooth, and cover the
   widescreen draw window.
4. **Setting.** A saved setting in the config owner, default off, off under oracle comparison. Guest state
   is never written; the off path is the current emitter unchanged.

## Proof

Each RE'd area at 4:3 and 16:9 with the setting on: horizon and shore match the original's, clouds and
waves move with the scroll at 60 fps (crops across consecutive presents), nothing else in the bucket
changes. Setting off: byte-identical to the current build.

## Constraints

No new art assets on disc or in git: clouds and waves are procedural shaders. No change to collision,
camera or any guest state.

## User direction (2026-11-13)

"For sea, you can find the existing water and expand it to cover everywhere." The sea is the game's own
existing water: find its drawer, mesh or strips and texture by RE, and extend that drawing to cover the whole
backdrop, instead of a host-drawn procedural sea. This replaces step 3's sea surface; the sky part of step 3
stands.

Design (coordinator, 2026-11-13; `docs/project-goals.md` G006): sea and sky and the later better-water mod share
one water owner. This mod decides where water is (the game's own water drawer's coverage extended over the
backdrop); better water (G006, its own setting) decides how it is drawn. Build the sea as that owner, so a
different draw of the same surfaces can slot in later.
