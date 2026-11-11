// tile_grid_layer.h — the field's scrolling 16x16 tile-grid backdrop (kNode): A00's scroll step
// FUN_8011534C and the area emitters of its sprite packets (A00 FUN_80115598 and its per-area variants, SOP
// FUN_8010C26C). The .cpp holds the RE.
#pragma once

#include "emit_memory.h"
#include "state_producer.h"
#include <cstdint>

struct Core;
class Game;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba {

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
  // SOP's routine without its palette-cycle tail, whose LoadImage uploads would refuse the replay running it.
  static void drawSop(Core &core, std::uint32_t node);
  // The grid node every emitter draws and the scroll step writes.
  inline static constexpr std::uint32_t kNode = 0x800ED018u;
  // A0B 0x801141B0 and its A0A/A0D/A0F copies: A00's emitter with no V bias (A0B's starry backdrop).
  static void emitUnbiased(Core *c);
  // A07 0x8012F938: no V bias, and its map rows start one row above the screen's top tile.
  static void emitRowLead(Core *c);
  // A06 0x80141A94: A07's walk over a map whose rows clamp instead of wrapping, hidden while the area phase is 0x13.
  static void emitClamped(Core *c);

  static void registerOverrides(native::NativeOverrideCatalog &natives);

  struct EmitGridArgs {
    uint32_t node;
    uint32_t tileVBias;
    int rowLead;    // map rows the walk starts above the screen's top tile
    bool clampRows; // rows past the map show its edge row instead of wrapping
    // How far past the scroll words the grid is scrolled (an in-between's), in pixels.
    float scrollPastX = 0.0f;
    float scrollPastY = 0.0f;
  };
  // The grid walk both emitters share, over the guest or a render's host memory: the sprites of every tile the
  // draw window shows, chained under a draw mode packet at the background bucket.
  static void emitGrid(const psx::present::EmitMemory &memory, const EmitGridArgs &args);

private:
  // `variant` indexes the emitters' bias and row lead.
  static void drawGrid(Core *c, std::uint32_t node, std::uint32_t variant);
  enum class SopTail : std::uint8_t { PaletteCycles, None };
  // FUN_8010C26C in its 88-byte frame: the grid, then `tail`.
  static void runSop(Core &core, std::uint32_t node, SopTail tail);
  // FUN_8010C26C's tail: three palette-cycle scripts, each loading one 16x1 CLUT row when its
  // countdown runs out. Uses the caller's frame for the RECT at sp+0x18.
  static void cycleSopPalettes(Core *c);
};

} // namespace tomba
