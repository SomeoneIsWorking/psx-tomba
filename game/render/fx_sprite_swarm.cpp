// game/render/fx_sprite_swarm.cpp — FUN_800281EC, extent [0x800281EC, 0x8002847C).
// Each particle at node+0x50 is projected and stamped on its own; the node's anchor at +0x2C is unused.
// The writer runs once per surviving particle, so its returned tail is dropped.
// The tail resolver 0x80031780 runs once up front instead, after the record head is captured.
// Scale is MAC0 * particle[6] >> 8; node[3] == '!' drops DQA from 6 to 4.
// The depth cue is cleared once before the loop.
// The particle loop is a do-while: a count of 0 still stamps particle 0.
#include "fx_sprite_swarm.h"
#include "core.h"
#include "core/overrides/guest_jal.h"
#include "core/overrides/native_override_catalog.h"
#include "game.h"
#include "guest_abi.h"

namespace {

constexpr uint32_t kSmallSizeClassTag = 0x21u; // '!'
constexpr uint32_t kDqaSmall = 4;
constexpr uint32_t kDqaNormal = 6;
constexpr int32_t kParticleSizeShift = 8;

constexpr uint32_t kListTailResolve = 0x80031780u;
constexpr uint32_t kSpriteWriter = 0x80027A4Cu;
constexpr uint32_t kRaListTailResolve = 0x80028318u;
constexpr uint32_t kRaSpriteWriter = 0x8002842Cu;

// 56-byte frame spilling s0..s7, fp and ra, in program order.
constexpr GuestFrameSpill kSpills[10] = {
    {17, 20},
    {21, 36},
    {31 /*ra*/, 52},
    {30, 48},
    {23, 44},
    {22, 40},
    {20, 32},
    {19, 28},
    {18, 24},
    {16, 16},
};

constexpr uint32_t kScratchpadBase = 0x1F800000u;

} // namespace

void FxSpriteSwarm::emitPerParticle(Core *c) {
  GuestFrame<56, 10> frame(c, kSpills);

  // Live across both calls in callee-saved registers, which the callees spill into their own frames.
  GuestReg<17> nodeReg(c);
  nodeReg = c->r[4];
  GuestReg<21> scaleXSlotReg(c);
  scaleXSlotReg = fxpublish::kScaleX;

  const FxSwarmNode node{c, c->r[17]};
  FxSpritePublish publish{c};

  if (node.recordHead() == 0) {
    return;
  }

  publish.loadSceneCamera(node.sizeClassTag() == kSmallSizeClassTag ? kDqaSmall : kDqaNormal);

  GuestReg<19> particleIndexReg(c);
  particleIndexReg = 0;
  GuestReg<18> particleReg(c);
  particleReg = node.particleArray();

  publish.setDepthCue(fxpublish::kDepthCueOff);

  // The resolver may clear node+0x34, and every particle stamps the list captured here.
  GuestReg<22> recordHeadReg(c);
  recordHeadReg = node.recordHead();

  c->r[4] = c->r[17];
  tomba::guest::dispatchJalToReturn(*c, kListTailResolve, kRaListTailResolve);

  // The guest also parks the scratchpad base and the publish slots in s0, s4, fp and s7.
  GuestReg<16> scratchpadBaseReg(c);
  scratchpadBaseReg = kScratchpadBase;
  GuestReg<20> otKeySlotReg(c);
  otKeySlotReg = fxpublish::kOtKey;
  GuestReg<30> scaleXSlotAliasReg(c);
  scaleXSlotAliasReg = fxpublish::kScaleX;
  GuestReg<23> screenXySlotReg(c);
  screenXySlotReg = fxpublish::kScreenXY;

  do {
    const FxSwarmParticle particle{c, c->r[18]};
    if (publish.projectAnchor(particle.worldXY(), particle.worldZ(), node.otBias())) {
      // mult leaves hi/lo guest-visible.
      guest_mult(c, publish.scaleX(), particle.sizeMul());
      const int32_t scale = (int32_t)c->lo >> kParticleSizeShift;

      c->r[4] = c->r[22];
      publish.setScaleX(scale);
      publish.setScaleY(scale);
      c->r[5] = (node.texturePage() << 16) | node.clut();
      tomba::guest::dispatchJalToReturn(*c, kSpriteWriter, kRaSpriteWriter);
    }

    particleIndexReg = particleIndexReg + 1;
    particleReg = particleReg + fxswarm::kParticleStride;
  } while ((int32_t)(int16_t)(uint32_t)particleIndexReg < node.particleCount());
}

void FxSpriteSwarm::registerOverrides(Game *) {
  tomba::native::declareOverride(0x800281ECu, "FxSpriteSwarm::emitPerParticle", &FxSpriteSwarm::emitPerParticle);
}
