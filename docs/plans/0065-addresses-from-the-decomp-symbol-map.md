# 0065 — Guest addresses from the decomp symbol map

Workspace rule (re-harness, 2026-11-13): RE knowledge lives in the decomp repository; the port reads guest addresses
from the decomp's symbol map as generated constants, never a hand copy. Tomba! 2's decomp is `external/tomba2-decomp`
(MIT, ours); its symbol map is `config/symbols/<image>.txt`, splat `symbol_addrs` format, one file per image
(`main`, `scus_944.54`, `start`..`crd`, `a00`..`a0l`).

Not started: it rewrites address constants across `game/`, which collides with work in flight. Run it when the tree
is quiet, one subsystem per commit.

## Today

- 501 `constexpr ... k<Name> = 0x80xxxxxx` in 108 files under `game/` (render 39 files, scene 14, ai 13, ui 7, frame 6,
  the rest 1 to 5), each a hand copy of an address with its meaning in a comment.
- 198 `natives.declare(...)` / `declareOverlay("Axx", ...)` calls with literal addresses and a hand-typed name.
- `docs/guest-addresses.md`, the address-to-owner index. Its names were seeded into the symbol map (2026-11-13);
  138 overlay rows have no recorded image (`external/tomba2-decomp/docs/unplaced-overlay-names.md`).

## Generator

One tool in psxport, shared by every title (Spyro's plan is the second consumer, so the parser has one
implementation): `tools/symbol_constants.py` reads a list of splat `symbol_addrs` files and writes one header per
image, `namespace <title>::sym::<image> { inline constexpr std::uint32_t <name> = 0x...u; }`, plus `size` where the
map gives one. A CMake function (`psxport_symbol_constants(<target> IMAGE <name> MAP <file>...)`) runs it at
configure/build time into `build/`, so the headers are never committed and a map edit rebuilds only their users.

- Names are emitted as the map spells them; a C++ identifier rule violation in the map is a map error, fixed in the
  decomp.
- Two names at one address, or one name at two addresses in one image, fail the generation.
- Struct field offsets are not addresses: they stay in the port until the decomp carries the structure (a header
  under `include/` there), then come from it the same way.
- Code sites that are not symbols (a branch target, a return address inside a function) are written as
  `<function> + 0xNN` against a map symbol, so the only literal left is the offset.

## Order

1. Write the tool and CMake function in psxport; build Tomba! 2 with it emitting headers nothing includes yet.
2. Natives: replace the 198 `declare`/`declareOverlay` literals with the generated constants, overlay image taken
   from the header's namespace, and the hand-typed name with the symbol's. Placing the 138 unplaced overlay names
   (their image) is part of this step, committed to the decomp first.
3. Constants, one subsystem per commit in this order: `game/core` (frame, sched, entry, assets, hle, overrides),
   `game/cd`, `game/audio`, `game/scene`, `game/world`, `game/player`, `game/camera`, `game/items`, `game/ui`,
   `game/ai`, `game/render`. Each constant either becomes a use of the generated name or, if the map lacks it, the
   name is added to the decomp map first (decomp commit, submodule bump in the same port change).
4. `docs/guest-addresses.md` keeps only the owner columns (address symbol, native owner); its name column is the
   map's symbol. Delete the index once `codemap.py find` can answer "who owns guest symbol X".
5. Build with Clang; run the dynamic scan through the title's warps once at the end to show no binding moved.
