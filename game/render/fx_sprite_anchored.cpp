// game/render/fx_sprite_anchored.cpp — FUN_80027CB4 and FUN_80027E5C, extents [0x80027CB4, 0x80027E5C)
// and [0x80027E5C, 0x8002801C).
// Both project the node's own anchor once and call the writer at most once, storing its tail at node+0x38.
// A culled node goes to the list tail resolver 0x80031780 instead of the writer, never to both.
// 0x80027CB4 draws the seaside hut-roof flames; its scale is MAC0 as published.
// 0x80027E5C rescales MAC0 by the node's size byte and republishes it to both axes.
// 0x80027E5C's only static caller is 0x80033080, the weapon-impact burst's render fn.
// These render fns are copied into node+0x18 from the descriptor table at 0x800A21C0, so no xref installs them.
#include "fx_sprite_anchored.h"
#include "core.h"
#include "core/overrides/guest_jal.h"
#include "core/overrides/native_override_catalog.h"
#include "game.h"
#include "guest_abi.h"

namespace {

constexpr uint32_t kDqaBase = 6;       // MAC0 = n * DQA, so this is the base sprite size
constexpr int32_t kScaleByteShift = 4; // node[6] is 4.4 fixed point, 16 = unity

constexpr uint32_t kListTailResolve = 0x80031780u;
constexpr uint32_t kSpriteWriter = 0x80027A4Cu;
constexpr uint32_t kRaUniformListTailResolve = 0x80027E20u;
constexpr uint32_t kRaUniformSpriteWriter = 0x80027E48u;
constexpr uint32_t kRaByteListTailResolve = 0x80027FC8u;
constexpr uint32_t kRaByteSpriteWriter = 0x80028008u;

// 24-byte frame spilling s0 and ra, the same for both members.
constexpr GuestFrameSpill kSpills[2] = {
    {16, 16},
    {31 /*ra*/, 20},
};

} // namespace

bool FxSpriteAnchored::projectNode(Core *c, uint32_t raListTailResolve) {
  const FxAnchoredNode node{c, c->r[16]};
  FxSpritePublish publish{c};
  if (node.recordHead() == 0) {
    return false;
  }
  publish.loadSceneCamera(kDqaBase);
  if (publish.projectAnchor(node.worldAnchorXY(), node.worldAnchorZ(), node.otBias())) {
    return true;
  }
  c->r[4] = c->r[16];
  tomba::guest::dispatchJalToReturn(*c, kListTailResolve, raListTailResolve);
  return false;
}

void FxSpriteAnchored::stamp(Core *c, uint32_t raSpriteWriter) {
  FxAnchoredNode node{c, c->r[16]};
  c->r[4] = node.recordHead();
  c->r[5] = (node.texturePage() << 16) | node.clut();
  tomba::guest::dispatchJalToReturn(*c, kSpriteWriter, raSpriteWriter);
  node.setRecordTail(c->r[2]);
}

void FxSpriteAnchored::emitUniformScale(Core *c) {
  GuestFrame<24, 2> frame(c, kSpills);
  GuestReg<16> nodeReg(c); // callees spill s0 into their own frames
  nodeReg = c->r[4];
  if (!projectNode(c, kRaUniformListTailResolve)) {
    return;
  }

  FxSpritePublish publish{c};
  const int32_t scale = publish.scaleX();
  publish.setDepthCue(fxpublish::kDepthCueOff);
  publish.setScaleY(scale);
  stamp(c, kRaUniformSpriteWriter);
}

void FxSpriteAnchored::emitByteScale(Core *c) {
  GuestFrame<24, 2> frame(c, kSpills);
  GuestReg<16> nodeReg(c); // callees spill s0 into their own frames
  nodeReg = c->r[4];
  if (!projectNode(c, kRaByteListTailResolve)) {
    return;
  }

  const FxAnchoredNode node{c, c->r[16]};
  FxSpritePublish publish{c};
  // mult leaves hi/lo guest-visible; only LO is shifted.
  const int64_t product = guest_mult(c, publish.scaleX(), (int32_t)node.scaleByte());
  const int32_t scale = (int32_t)(uint32_t)product >> kScaleByteShift;
  publish.setDepthCue(fxpublish::kDepthCueOff);
  publish.setScaleX(scale);
  publish.setScaleY(scale);
  stamp(c, kRaByteSpriteWriter);
}

void FxSpriteAnchored::registerOverrides(Game *) {
  tomba::native::declareOverride(
      0x80027CB4u, "FxSpriteAnchored::emitUniformScale", &FxSpriteAnchored::emitUniformScale);
  tomba::native::declareOverride(0x80027E5Cu, "FxSpriteAnchored::emitByteScale", &FxSpriteAnchored::emitByteScale);
}
