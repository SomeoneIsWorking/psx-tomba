// class Render — Tomba! 2's guest-time render ports, one instance per Core, reached as `rend(c)->...`.
//
// Every method here is a native body of a guest render function: it writes the same guest state
// (GTE registers, scratchpad, packet pool, ordering table) the guest body writes. The picture is the
// guest's GP0 output on the GPU device; nothing here draws to the host.
//
// Owned by Core via a pointer (`Core::mRender`), constructed in game_ctx.cpp.
#pragma once
#include "margin_render.h"
#include "node_xform.h"
#include <cstdint>
class Core;

class Render {
public:
  Core *mCore = nullptr;

  NodeXform mNodeXform;  // scene-node world-transform builder (guest FUN_80051844)
  MarginRenderer margin; // widescreen margin re-include set (collected in the cull)

  // Per-frame render orchestrators: guest 0x8003F9A8 (field) and 0x8003FA44 (mid-transition).
  void frame();
  void frameX();

  // FUN_8003F698's routing rule: the guest emitter a cmd with this `flag` reaches. `*caseLabelOut` is
  // the jump-table label used, or 0 for the generic emitter.
  uint32_t resolvePerModeEmitter(Core *c, uint32_t flag, uint32_t *caseLabelOut);

  // FUN_8003CDD8 (a0=node, a1=flag): composes each active render command's world transform into
  // GTE CR0-7 and dispatches it through perModeDispatch.
  void cmdListDispatch();
  // FUN_8003F698 (a0=geomblk, a1=otbase, a2=flag): the area's per-mode renderer, or the generic
  // GT3/GT4 emitter 0x800803DC.
  void perModeDispatch();

  // FUN_8003CCA4 (a0=node): per-object render-type dispatch; every case runs cmdListDispatch and four
  // of them then rewrite the emitted packet span with an effect modifier below.
  void perObjRenderDispatch();
  // Billboard composes (a0=node): build the node's local matrix, compose the camera onto its world
  // position, then billboardEmit. FUN_8003C2D4 / 8003C464 / 8003C788 / 8003C5F8.
  void billboardCompose1();
  void billboardCompose2();
  void billboardCompose3();
  void billboardComposeC5F8();
  // FUN_8003C8F4 (a0=node, a1=flag): projects each particle of the node's active sub-list and links a
  // GT4-style packet into the OT.
  void billboardEmit();

  // Effect modifiers over a just-emitted packet-pool span [lo,hi) (effect_mod.cpp).
  void effectSemiOn(uint32_t node, uint32_t lo, uint32_t hi);   // FUN_8003F3F4
  void effectSemiOff(uint32_t node, uint32_t lo, uint32_t hi);  // FUN_8003F4C4
  void effectClutSwap(uint32_t node, uint32_t lo, uint32_t hi); // FUN_8003F344
  void effectFlatTint(uint32_t node, uint32_t lo, uint32_t hi); // FUN_8003F594
  void effectColorAdd(uint32_t node, uint32_t lo, uint32_t hi); // FUN_8003D584

  static void composeTintGate(Core *c);     // FUN_8003EF9C — per-type render gate
  static void subPartWalk(Core *c);         // FUN_8003F174 — articulated sub-part submit
  static void sharedTransformWalk(Core *c); // FUN_8003F07C — rigid sub-part submit

  void textLabelEmit(); // FUN_80039F4C — per-character 3D text label

  void renderWalk();          // FUN_8003C048 — the render-node list walk (node+0x18 render fns)
  void overlayTypeDispatch(); // FUN_8003D0BC — per-area-type overlay dispatch

  // libgpu ports (wide_re_gpu_*.cpp, wide_re_libgpu_leaves.cpp).
  void gpuDmaQueueEnqueue(); // FUN_80082D04
  void gpuDmaQueueDrain();   // FUN_80082FB4
  void gpuDmaQueueSync();    // FUN_80083364
  void gpuDmaSend();         // FUN_80082424
  void drawSync();           // FUN_80080F6C
  void clearOTagR();         // FUN_80081458
  void gpuLoadImageStream(); // FUN_80082734

  // Object-list walkers reached from FUN_8003F9A8 (objlist_walk.cpp).
  void objListWalk1(); // FUN_8003BB50
  void objListWalk2(); // FUN_8003BCF4
  void objListWalk3(); // FUN_8003BF00
  void objListWalk4(); // FUN_8003EEC0
};
