// game/render/fx_sprite_swarm.cpp — FxSpriteSwarm, extent [0x800281EC, 0x8002847C). See fx_sprite_swarm.h.
// The tail resolver 0x80031780 runs once up front; every particle stamps the captured head; node[3] '!' sets DQA 4.
#include "fx_sprite_swarm.h"
#include "core/overrides/guest_jal.h"
#include "core/overrides/native_override_catalog.h"
#include "entry/session.h"
#include "fx_sprite_publish.h"
#include "guest_abi.h"
#include "guest_ordering_table.h"
#include "list_state_producer.h"
#include "scaled_sprites.h"

#include <algorithm>
#include <memory>

namespace tomba {

namespace {

constexpr uint32_t kSmallSizeClassTag = 0x21u; // '!'
constexpr uint32_t kDqaSmall = 4;
constexpr uint32_t kDqaNormal = 6;
constexpr int32_t kParticleSizeShift = 8;

// Node fields.
constexpr uint32_t kSizeClass = 0x03;     // u8 node tag; '!' selects the small size class
constexpr uint32_t kOtBias = 0x32;        // s16 added to the OT bucket key
constexpr uint32_t kRecordHead = 0x34;    // u32 sprite-record list, the writer's a0
constexpr uint32_t kTextureClut = 0x44;   // u32 the writer's a1: CLUT (lo16), texture page (hi16)
constexpr uint32_t kParticleCount = 0x4E; // s16 live particles
constexpr uint32_t kParticleArray = 0x50; // particle records, stride 8

constexpr uint32_t kParticleStride = 8;
constexpr uint32_t kPartPosXY = 0x00;   // u32 packed world VX (lo16) | VY (hi16)
constexpr uint32_t kPartPosZ = 0x04;    // u32 world VZ in the low half; the high half is kPartSizeMul
constexpr uint32_t kPartSizeMul = 0x06; // s16 particle size, 8.8 fixed point

constexpr uint32_t kListTailResolve = 0x80031780u;
constexpr uint32_t kRaListTailResolve = 0x80028318u;

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

// The particles the do-while stamps.
uint32_t particleCount(const psx::present::EmitMemory &memory, uint32_t node) {
  return (uint32_t)std::max<int32_t>(memory.mem_r16s(node + kParticleCount), 1);
}

void emitPerParticle(Core *c) {
  GuestFrame<56, 10> frame(c, kSpills);

  // Live across both calls in callee-saved registers, which the callees spill into their own frames.
  GuestReg<17> nodeReg(c);
  nodeReg = c->r[4];
  GuestReg<21> scaleXSlotReg(c);
  scaleXSlotReg = fxpublish::kScaleX;
  const uint32_t node = c->r[17];
  const uint32_t records = c->mem_r32(node + kRecordHead);
  // The resolver may clear node+0x34, and every particle stamps the list captured here.
  const render::ListCall call{.list = node,
                              .ot = c->mem_r32(render::OrderingTable::kBasePointer),
                              .count = 0,
                              .a3 = records,
                              .sp = c->r[29],
                              .a1 = 0};
  const render::EmitterObject object(
      *c, FxSpriteSwarm::kEmitter, render::OwnObject{eng(c).graphicsBind.records.object(node)});
  const uint32_t particles = particleCount(*c, node);
  const uint32_t arenaBytes =
      records == 0 ? 0
                   : particles * render::ScaledSprites::recordCount(*c, records) * render::ScaledSprites::kPacketBytes;
  render::ListJobWriter job(*c, 0, call, arenaBytes);
  keepSceneCamera(job, *c);
  job.input(*c, node, kParticleArray);
  job.input(*c, node + kParticleArray, particles * kParticleStride, render::InputBlend::Lanes16);

  if (!FxSpriteSwarm::begin(*c, call)) {
    return;
  }

  GuestReg<19> particleIndexReg(c);
  particleIndexReg = 0;
  GuestReg<18> particleReg(c);
  particleReg = node + kParticleArray;
  GuestReg<22> recordHeadReg(c);
  recordHeadReg = records;

  c->r[4] = node;
  tomba::guest::dispatchJalToReturn(*c, {.target = kListTailResolve, .returnAddress = kRaListTailResolve});

  // The guest also parks the scratchpad base and the publish slots in s0, s4, fp and s7; the writer spills s0, s1.
  GuestReg<16> scratchpadBaseReg(c);
  scratchpadBaseReg = kScratchpadBase;
  GuestReg<20> otKeySlotReg(c);
  otKeySlotReg = fxpublish::kOtKey;
  GuestReg<30> scaleXSlotAliasReg(c);
  scaleXSlotAliasReg = fxpublish::kScaleX;
  GuestReg<23> screenXySlotReg(c);
  screenXySlotReg = fxpublish::kScreenXY;

  FxSpriteSwarm::particles(*c, call);
  job.save(*c);
}

class SwarmProducer final : public render::ListStateProducer {
public:
  using ListStateProducer::ListStateProducer;

protected:
  void emit(const psx::present::EmitMemory &memory, uint32_t, const render::ListCall &call) const override {
    if (FxSpriteSwarm::begin(memory, call)) {
      FxSpriteSwarm::particles(memory, call);
    }
  }
};

} // namespace

bool FxSpriteSwarm::begin(const psx::present::EmitMemory &memory, const render::ListCall &call) {
  if (call.a3 == 0) {
    return false;
  }
  const FxSpritePublish publish{memory};
  publish.loadSceneCamera(memory.mem_r8(call.list + kSizeClass) == kSmallSizeClassTag ? kDqaSmall : kDqaNormal);
  publish.setDepthCue(fxpublish::kDepthCueOff);
  return true;
}

void FxSpriteSwarm::particles(const psx::present::EmitMemory &memory, const render::ListCall &call) {
  const FxSpritePublish publish{memory};
  const uint32_t node = call.list;
  const uint32_t particles = particleCount(memory, node);
  for (uint32_t index = 0; index < particles; index++) {
    const uint32_t particle = node + kParticleArray + index * kParticleStride;
    if (!publish.projectAnchor({.worldXY = memory.mem_r32(particle + kPartPosXY),
                                .worldZ = memory.mem_r32(particle + kPartPosZ),
                                .otBias = memory.mem_r16s(node + kOtBias)})) {
      continue;
    }
    // mult leaves hi/lo guest-visible.
    const auto product = (int32_t)(uint32_t)psx::cpu::multiply(
        &memory.core(), publish.scaleX(), memory.mem_r16s(particle + kPartSizeMul));
    publish.setScaleX(product >> kParticleSizeShift);
    publish.setScaleY(product >> kParticleSizeShift);
    render::ScaledSprites::stamp(
        memory, {.records = call.a3, .textureClut = memory.mem_r32(node + kTextureClut), .ot = call.ot});
  }
}

void FxSpriteSwarm::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declare(kEmitter, "FxSpriteSwarm::emitPerParticle", &emitPerParticle);
}

void FxSpriteSwarm::registerStateRenders(psx::present::StateProducerTable &renders, Core &core) {
  renders.install(kEmitter, std::make_unique<SwarmProducer>(core));
}

} // namespace tomba
