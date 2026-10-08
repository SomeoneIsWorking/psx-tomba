// game/render/tile_grid_layer.h — PC-native (dual-emit) body for the field's scrolling 16x16
// tile-grid background: the scroll-wrap helper FUN_8011534C and the op-0x7C sprite-packet emitter
// FUN_80115598 (A00 overlay, guest node @ 0x800ED018). See tile_grid_layer.cpp for the full RE +
// entry-point-resolution trace (docs/native-render-2d-tilegrid.md).
//
// NOT the same call chain as the RE doc originally guessed: docs/native-render-2d-tilegrid.md's
// "Function boundaries" section names 0x80115364 (Render::overlayTypeDispatch case 0x8003D1C4) as
// the dispatch entry. That address is NOT a real function entry (see .cpp "entry resolution") and
// that call path is dead for the field. The REAL, live callers are:
//   - Engine::areaModeDispatch(Faithful) (game/core/engine.cpp, mode idx 0) -> typed runtime address dispatch(c,
//     0x8011534Cu) with a0=0x800ED018 — per-frame STEP (scroll wrap).
//   - the still-unowned FUN_8003DF04 state dispatcher -> typed runtime address dispatch(c, 0x80115598u) with
//     a0=0x800ED018 for render-state 0 — per-frame EMIT.
// This class owns those TWO leaf bodies directly (wired via tomba::native::declareOverride, the SAME
// mechanism OverlayGroundGt3Gt4 uses for its A00-local leaves — neither is reached via typed runtime address dispatch
// from a literal MAIN jal, both are reached through per-node function-pointer / table dispatch that
// resolves to these two addresses at runtime).
//
#pragma once
#include <cstdint>

struct Core;
class Game;

class TileGridLayer {
public:
  static void scrollStep(Core *c); // FUN_8011534C(node=a0) — state 0: one-time init from
                                   // *0x800ECF84; state 1: recompute wrapped scroll X/Y into
                                   // node+0x28/+0x2A from scratchpad 0x1F8000F2/F0.
  static void emit(Core *c);       // FUN_80115598(node=a0) — walk the W×H tile grid, emit one
                                   // 16-byte op-0x7C sprite packet per visible tile into the
                                   // shared packet pool, splice into OT bucket
                                   // 0x7FF, append a trailing DR_TPAGE reset packet via the
                                   // already-RE'd guest 0x80083DE0.
  static void emitSop(Core *c);    // SOP FUN_8010C26C(node=a0): the same grid with no V bias, then
                                   // the three CLUT-row palette cycles (cycleSopPalettes).
  // A0B 0x801141B0 and its A0A/A0D/A0F copies: A00's emitter with no V bias (A0B's starry backdrop).
  static void emitUnbiased(Core *c);

  static void registerOverrides(Game *game);

private:
  // The grid walk both emitters share; each tile is keyed by its map cell. Runs inside the caller's
  // guest frame (the trailing 0x80083DE0 call takes its 5th argument at sp+16).
  static void emitGrid(Core *c, std::uint32_t node, std::uint32_t tileVBias);
  // FUN_8010C26C's tail: three palette-cycle scripts, each loading one 16x1 CLUT row when its
  // countdown runs out. Uses the caller's frame for the RECT at sp+0x18.
  static void cycleSopPalettes(Core *c);
};
