// game/render/fx_sprite_anchored.cpp — FxSpriteAnchored. See fx_sprite_anchored.h.
// Extents [0x80027CB4, 0x800281EC), [0x800286CC, 0x800288AC) and [0x8002DBE4, 0x8002DD88).
#include "fx_sprite_anchored.h"
#include "core/overrides/guest_jal.h"
#include "core/overrides/native_override_catalog.h"
#include "entry/session.h"
#include "fx_sprite_publish.h"
#include "guest_abi.h"
#include "guest_ordering_table.h"
#include "list_state_producer.h"
#include "packed_mesh.h"
#include "scaled_sprites.h"

#include <iterator>
#include <memory>

namespace tomba {

namespace {

using render::InputBlend;

constexpr uint32_t kDqaBase = 6; // MAC0 = n * DQA, so this is the base sprite size

// Node fields.
constexpr uint32_t kScaleByte = 0x06;     // u8 size multiplier, 4.4 fixed point
constexpr uint32_t kCueByte = 0x07;       // u8 depth cue, << 4 into IR0
constexpr uint32_t kWorldAnchorXY = 0x2C; // u32 packed world VX (lo16) | VY (hi16)
constexpr uint32_t kWorldAnchorZ = 0x30;  // u32 world VZ in the low half; the high half is kOtBias
constexpr uint32_t kOtBias = 0x32;        // s16 added to the OT bucket key
constexpr uint32_t kRecordHead = 0x34;    // u32 sprite-record list; 0x800286CC: u16 x and y scales
constexpr uint32_t kRecordTail = 0x38;    // u32 record the writer returns
constexpr uint32_t kScript = 0x3C;        // u32 0x800286CC frame script cursor
constexpr uint32_t kScriptNext = 0x40;    // u32 0x800286CC next script byte, 0 to hold the frame
constexpr uint32_t kTextureClut = 0x44;   // u32 the writer's a1: CLUT (lo16), texture page (hi16)
constexpr uint32_t kScalePair = 0x48;     // s16 x and y scales of 0x8002801C
constexpr uint32_t kFrameTable = 0x50;    // u32 0x800286CC frame lists
constexpr uint32_t kNodeBytes = 0x54;

constexpr uint32_t kHoldFrame = 0x80u;
constexpr int32_t kScaleByteShift = 4;
constexpr int32_t kScalePairShift = 8;

constexpr uint32_t kListTailResolve = 0x80031780u;

// 24-byte frame spilling s0 and ra; 0x800286CC spills ra only.
constexpr GuestFrameSpill kSpills[2] = {
    {16, 16},
    {31 /*ra*/, 20},
};
constexpr GuestFrameSpill kQuadSpills[1] = {{31 /*ra*/, 16}};

FxSpriteAnchored::Variant variantOf(uint32_t emitter) {
  for (uint32_t index = 0; index < std::size(FxSpriteAnchored::kEmitters); index++) {
    if (FxSpriteAnchored::kEmitters[index] == emitter) {
      return static_cast<FxSpriteAnchored::Variant>(index);
    }
  }
  return FxSpriteAnchored::Variant::Uniform;
}

uint32_t emitterOf(FxSpriteAnchored::Variant variant) {
  return FxSpriteAnchored::kEmitters[static_cast<uint32_t>(variant)];
}

struct AnchoredNode {
  psx::present::EmitMemory mMemory;
  uint32_t mBase;

  uint32_t recordHead() const {
    return mMemory.mem_r32(mBase + kRecordHead);
  }
  // 0x800286CC: the script cursor, 0 for no sprite.
  uint32_t script() const {
    return mMemory.mem_r32(mBase + kScript);
  }
  struct ScriptStep {
    uint32_t frame;
    uint32_t next;
  };
  ScriptStep scriptStep() const {
    const uint32_t cursor = script();
    const uint32_t step = mMemory.mem_r8(cursor);
    if ((step & kHoldFrame) != 0u) {
      return {step & ~kHoldFrame, 0};
    }
    return {step, cursor + 1u};
  }
  uint32_t frameRecords(uint32_t frame) const {
    return mMemory.mem_r32(mMemory.mem_r32(mBase + kFrameTable) + frame * 4u);
  }
  FxSpritePublish::ProjectAnchorArgs anchor() const {
    return {.worldXY = mMemory.mem_r32(mBase + kWorldAnchorXY),
            .worldZ = mMemory.mem_r32(mBase + kWorldAnchorZ),
            .otBias = mMemory.mem_r16s(mBase + kOtBias)};
  }
};

// The most packet bytes the emitter writes this frame.
uint32_t arenaBytes(const psx::present::EmitMemory &memory, FxSpriteAnchored::Variant variant, uint32_t node) {
  const AnchoredNode view{memory, node};
  if (variant == FxSpriteAnchored::Variant::AnimatedQuads) {
    if (view.script() == 0) {
      return 0;
    }
    return render::PackedRecord::count(memory, view.frameRecords(view.scriptStep().frame)) *
           render::PackedMesh::kPacketBytes;
  }
  if (view.recordHead() == 0) {
    return 0;
  }
  return render::ScaledSprites::recordCount(memory, view.recordHead()) * render::ScaledSprites::kPacketBytes;
}

// The node as a render reads it: its anchor, and the scales a variant takes from it, move between frames.
void keepNode(render::ListJobWriter &job, Core &core, FxSpriteAnchored::Variant variant, uint32_t node) {
  job.input(core, node, kWorldAnchorXY);
  job.input(core, node + kWorldAnchorXY, 6, InputBlend::Lanes16);
  job.input(core, node + kOtBias, 2);
  if (variant == FxSpriteAnchored::Variant::AnimatedQuads) {
    job.input(core, node + kRecordHead, 4, InputBlend::Lanes16);
    job.input(core, node + kRecordTail, kNodeBytes - kRecordTail);
  } else if (variant == FxSpriteAnchored::Variant::ScalePair) {
    job.input(core, node + kRecordHead, kScalePair - kRecordHead);
    job.input(core, node + kScalePair, 4, InputBlend::Lanes16);
    job.input(core, node + kScalePair + 4, kNodeBytes - kScalePair - 4);
  } else {
    job.input(core, node + kRecordHead, kNodeBytes - kRecordHead);
  }
}

// The emitter as the guest calls it, saving its call when it stamps and handing a culled node to `resolveReturn`.
void drawForGuest(Core *c, FxSpriteAnchored::Variant variant, uint32_t node, uint32_t resolveReturn) {
  const render::ListCall call{.list = node,
                              .ot = c->mem_r32(render::OrderingTable::kBasePointer),
                              .count = 0,
                              .a3 = 0,
                              .sp = c->r[29],
                              .a1 = 0};
  FxSpriteAnchored::Drawn drawn = FxSpriteAnchored::Drawn::Nothing;
  {
    const render::EmitterObject object(
        *c, emitterOf(variant), render::OwnObject{eng(c).graphicsBind.records.object(node)});
    render::ListJobWriter job(*c, static_cast<uint32_t>(variant), call, arenaBytes(*c, variant, node));
    keepSceneCamera(job, *c);
    keepNode(job, *c, variant, node);
    drawn = FxSpriteAnchored::draw(*c, variant, call);
    if (drawn == FxSpriteAnchored::Drawn::Stamped) {
      job.save(*c);
    }
  }
  if (drawn == FxSpriteAnchored::Drawn::Culled && resolveReturn != 0) {
    c->r[4] = node;
    tomba::guest::dispatchJalToReturn(*c, {.target = kListTailResolve, .returnAddress = resolveReturn});
  }
}

// Emitters whose frame spills s0 and ra, with the node in s0 for the callees to spill.
template <FxSpriteAnchored::Variant Variant, uint32_t ResolveReturn> void nodeInS0(Core *c) {
  GuestFrame<24, 2> frame(c, kSpills);
  GuestReg<16> nodeReg(c);
  nodeReg = c->r[4];
  drawForGuest(c, Variant, c->r[16], ResolveReturn);
}

void animatedQuads(Core *c) {
  GuestFrame<24, 1> frame(c, kQuadSpills);
  drawForGuest(c, FxSpriteAnchored::Variant::AnimatedQuads, c->r[4], 0);
}

class AnchoredSpriteProducer final : public render::ListStateProducer {
public:
  using ListStateProducer::ListStateProducer;

protected:
  void emit(const psx::present::EmitMemory &memory, uint32_t variant, const render::ListCall &call) const override {
    FxSpriteAnchored::draw(memory, static_cast<FxSpriteAnchored::Variant>(variant), call);
  }
};

} // namespace

FxSpriteAnchored::Drawn
FxSpriteAnchored::draw(const psx::present::EmitMemory &memory, Variant variant, const render::ListCall &call) {
  const AnchoredNode node{memory, call.list};
  const FxSpritePublish publish{memory};
  Core *core = &memory.core();
  uint32_t frame = 0;
  if (variant == Variant::AnimatedQuads) {
    if (node.script() == 0) {
      return Drawn::Nothing;
    }
    const AnchoredNode::ScriptStep step = node.scriptStep();
    memory.mem_w32(call.list + kScriptNext, step.next);
    frame = step.frame;
  } else if (node.recordHead() == 0) {
    return Drawn::Nothing;
  }
  publish.loadSceneCamera(kDqaBase);
  if (!publish.projectAnchor(node.anchor())) {
    return Drawn::Culled;
  }
  const int32_t scale = publish.scaleX();
  switch (variant) {
  case Variant::Uniform:
    publish.setDepthCue(fxpublish::kDepthCueOff);
    publish.setScaleY(scale);
    break;
  case Variant::ByteScale: {
    // mult leaves hi/lo guest-visible; only LO is shifted.
    const auto product =
        (int32_t)(uint32_t)psx::cpu::multiply(core, scale, (int32_t)memory.mem_r8(call.list + kScaleByte));
    publish.setDepthCue(fxpublish::kDepthCueOff);
    publish.setScaleX(product >> kScaleByteShift);
    publish.setScaleY(product >> kScaleByteShift);
    break;
  }
  case Variant::ScalePair: {
    const auto y =
        (int32_t)psx::cpu::multiply(core, scale >> kScalePairShift, memory.mem_r16s(call.list + kScalePair + 2));
    const auto x = (int32_t)psx::cpu::multiply(core, scale >> kScalePairShift, memory.mem_r16s(call.list + kScalePair));
    publish.setDepthCue(fxpublish::kDepthCueOff);
    publish.setScaleY(y);
    publish.setScaleX(x);
    break;
  }
  case Variant::Cued:
    publish.setScaleY(scale);
    publish.setDepthCue((int32_t)(memory.mem_r8(call.list + kCueByte) << 4));
    break;
  case Variant::AnimatedQuads: {
    const uint32_t scales = memory.mem_r32(call.list + kRecordHead);
    const auto y = (uint32_t)psx::cpu::multiply(core, scale, (int32_t)(scales >> 16));
    const auto x = (uint32_t)psx::cpu::multiply(core, scale, (int32_t)(scales & 0xFFFFu));
    publish.setDepthCue(fxpublish::kDepthCueOff);
    publish.setScaleY((int32_t)(y >> kScalePairShift));
    publish.setScaleX((int32_t)(x >> kScalePairShift));
    render::PackedMesh::emitScreen(memory, {.list = node.frameRecords(frame), .clutRows = 0, .ot = call.ot});
    return Drawn::Stamped;
  }
  }
  const uint32_t tail = render::ScaledSprites::stamp(
      memory, {.records = node.recordHead(), .textureClut = memory.mem_r32(call.list + kTextureClut), .ot = call.ot});
  memory.mem_w32(call.list + kRecordTail, tail);
  return Drawn::Stamped;
}

void FxSpriteAnchored::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declare(kEmitters[0], "FxSpriteAnchored::uniform", &nodeInS0<Variant::Uniform, 0x80027E20u>);
  natives.declare(kEmitters[1], "FxSpriteAnchored::byteScale", &nodeInS0<Variant::ByteScale, 0x80027FC8u>);
  natives.declare(kEmitters[2], "FxSpriteAnchored::scalePair", &nodeInS0<Variant::ScalePair, 0x80028188u>);
  natives.declare(kEmitters[3], "FxSpriteAnchored::cued", &nodeInS0<Variant::Cued, 0u>);
  natives.declare(kEmitters[4], "FxSpriteAnchored::animatedQuads", &animatedQuads);
}

void FxSpriteAnchored::registerStateRenders(psx::present::StateProducerTable &renders, Core &core) {
  for (const uint32_t emitter : kEmitters) {
    renders.install(emitter, std::make_unique<AnchoredSpriteProducer>(core));
  }
}

} // namespace tomba
