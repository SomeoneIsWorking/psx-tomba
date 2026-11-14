#include "frame/draw_motion.h"

#include "camera/cutscene_camera.h"
#include "core.h"
#include "guest_redraw.h"
#include "render/sway_model_emitter.h"
#include "render/tile_grid_layer.h"
#include "render/unlit_model_emitter.h"

#include <lucent/log.h>

#include <cmath>
#include <utility>

namespace tomba {
namespace {

using psx::present::Motion;
using psx::present::MotionField;
using psx::present::MotionKey;
using psx::present::MotionRecords;
using psx::present::MotionTrack;
using psx::present::RecordByte;

// The view MATRIX every step loads into the GTE.
constexpr std::uint32_t kView = 0x1F8000F8u;
constexpr auto kViewFields = psx::present::transformFields(0);
// Camera pitch and yaw, s16 each.
constexpr std::uint32_t kViewAngles = 0x1F8000F0u;
constexpr std::array kViewAngleFields{MotionField{.offset = 0, .motion = Motion::Angle12x2}};

// The eye and target words the look-at reads the whole units of, 16.16: eye X, Y, Z at +0, +4, +8, target at +0xC.
constexpr std::uint32_t kLookInputs = camera::kCamScratch;
constexpr std::uint32_t kViewTranslation = kView + 0x14u;
constexpr std::size_t kRotationWords = 5;

// Node position s32 16.16 at +0x2C, angles s16 at +0x54.
constexpr std::array kNodeFields{
    MotionField{.offset = 0x2C, .motion = Motion::Linear},
    MotionField{.offset = 0x30, .motion = Motion::Linear},
    MotionField{.offset = 0x34, .motion = Motion::Linear},
    MotionField{.offset = 0x54, .motion = Motion::Angle12x2},
    MotionField{.offset = 0x58, .motion = Motion::Angle12x2},
};
// Effect node X|Y and Z|OT bias as s16 halves; +0x34 is a list or a scale pair by emitter and steps.
constexpr std::array kEffectNodeFields{
    MotionField{.offset = 0x2C, .motion = Motion::Halves},
    MotionField{.offset = 0x30, .motion = Motion::Halves},
};
// 0x800597AC rebuilds Tomba's record transforms from his node inside the draw.
constexpr std::uint32_t kTomba = 0x800E7E80u;
// His node fields, then the animation's root offset s16 x3 at +0x88 that 0x800597AC turns by his body matrix;
// FUN_80075F0C steps it a tick and FUN_80076904 loads it at each keyframe.
constexpr std::array kTombaFields{
    kNodeFields[0],
    kNodeFields[1],
    kNodeFields[2],
    kNodeFields[3],
    kNodeFields[4],
    MotionField{.offset = 0x88, .motion = Motion::Halves},
    MotionField{.offset = 0x8C, .motion = Motion::Low16},
};
// The word holding his pan +0x14E in its high half, then the one holding the +0x177 flags.
constexpr std::array kTombaPanFields{MotionField{.offset = 0x14C}, MotionField{.offset = 0x174}};
constexpr std::uint32_t kTombaPanned = 1u << 24;
// The typed node pools of FUN_800798F8, live while +1 is set.
struct NodePool {
  std::uint32_t base = 0;
  std::uint32_t stride = 0;
  std::uint32_t count = 0;
};
constexpr NodePool kEffectNodes{0x800ED8D8u, 0x88u, 0x34u};
constexpr std::array kNodePools{NodePool{0x800EF478u, 0xC4u, 0x3Au},
                                NodePool{0x800FE198u, 0xD0u, 0x2Au},
                                NodePool{0x800FB858u, 0x108u, 0x28u},
                                NodePool{0x800FB218u, 0x140u, 0x05u}};
// Node world MATRIX at +0x98; A03's ride draw 0x80109870 poses Tomba from his cart's.
constexpr auto kNodeTransform = psx::present::transformFields(0x98);
constexpr std::array kNodeKeys{MotionKey{.offset = 0x08, .mask = 0xFF00FF00u}};
constexpr RecordByte kNodeLive{.offset = 1, .low = 1, .high = 0xFF};

// Render records (node +0xC0 + 4i): world MATRIX +0x18, part angles s16 +0x8, model +0x40.
using world::RenderRecordPool;
constexpr auto kRecordTransform = psx::present::transformFields(0x18);
constexpr std::array kRecordAngles{MotionField{.offset = 0x08, .motion = Motion::Angle12x2},
                                   MotionField{.offset = 0x0C, .motion = Motion::Angle12Low}};
constexpr std::array kRecordKeys{MotionKey{.offset = 0x40}};

// The effect table M:8003F024 walks: live while +0 is set, model byte +7 (M:8003D23C).
constexpr std::uint32_t kEffects = 0x800EC188u;
constexpr MotionRecords kEffectRun{
    .stride = 0x40u, .count = 40u, .live = RecordByte{.offset = 0, .low = 1, .high = 0xFF}};
constexpr std::array kEffectKeys{MotionKey{.offset = 0x04, .mask = 0xFF000000u}};
constexpr std::array kEffectFields{
    MotionField{.offset = 0x0C, .motion = Motion::Low16}, // scale; +0xE is the code byte
    MotionField{.offset = 0x1C, .motion = Motion::Linear},
    MotionField{.offset = 0x20, .motion = Motion::Linear},
    MotionField{.offset = 0x24, .motion = Motion::Linear},
    MotionField{.offset = 0x28, .motion = Motion::Angle12x2},
    MotionField{.offset = 0x2C, .motion = Motion::Angle12Low},
};

// The tile grid 0x800ED018 (TileGridLayer): the update's 0x8011534C wraps scroll X|Y at +0x28 into pitch X|Y at
// +0x30 from the view angles; live in state 1.
constexpr std::uint32_t kTileGrid = TileGridLayer::kNode;
constexpr std::array kTileGridFields{MotionField{.offset = 0x28}, MotionField{.offset = 0x30}};
constexpr RecordByte kTileGridLive{.offset = 0, .low = 1, .high = 1};
// A01's sway phase, a 12-bit angle the walker A01:80132358 steps before its leaves read it.
constexpr std::uint32_t kA01Sway = render::SwayModelEmitter::kSwayPhase;
constexpr std::array kAngleLow{MotionField{.offset = 0, .motion = Motion::Angle12Low}};
// UV scrolls the area objects step in the update, read as u16 >> 4: A02's U (& 0xFF) | V (mod 0x180) and band
// (+0x100 mod 0x800); A06's U (& 0xFF).
constexpr std::uint32_t kA02Scroll = render::UnlitModelEmitter::kA02ScrollU;
static_assert(render::UnlitModelEmitter::kA02ScrollV == kA02Scroll + 2u);
constexpr std::uint32_t kA02Band = render::UnlitModelEmitter::kA02Band;
constexpr std::uint32_t kA06Scroll = render::UnlitModelEmitter::kA06ScrollU;
// A0F's U scroll, -2 & 0x3F a draw, after the walker A0F:80115364 drew with it; read as a byte, the word's high half.
constexpr std::uint32_t kA0fScroll = render::UnlitModelEmitter::kA0fScrollU - 2u;
constexpr std::array kWord{MotionField{.offset = 0}};

constexpr std::array<const char *, kNodePools.size()> kNodePoolNames{
    "nodes 0xC4", "nodes 0xD0", "nodes 0x108", "nodes 0x140"};
constexpr std::array<const char *, kNodePools.size()> kNodeTransformNames{
    "node transforms 0xC4", "node transforms 0xD0", "node transforms 0x108", "node transforms 0x140"};

} // namespace

DrawMotion::DrawMotion(world::RenderRecordPool &records) {
  // No byte marks a free record: FUN_8007AAE8 and FUN_8007ADDC only move the free stack.
  const MotionRecords recordRun{
      .stride = RenderRecordPool::kRecordStride, .count = RenderRecordPool::kRecordCount, .liveness = &records};
  std::size_t at = 0;
  tracks_[at++] = {.name = "view", .address = kView, .fields = kViewFields, .rule = &view_};
  tracks_[at++] = {.name = "view angles", .address = kViewAngles, .fields = kViewAngleFields};
  tracks_[at++] = {.name = "tomba", .address = kTomba, .fields = kTombaFields};
  tracks_[at++] = {.name = "tomba pan", .address = kTomba, .fields = kTombaPanFields, .rule = &tombaPan_};
  tracks_[at++] = {.name = "effect nodes",
                   .address = kEffectNodes.base,
                   .fields = kEffectNodeFields,
                   .keys = kNodeKeys,
                   .records = {.stride = kEffectNodes.stride, .count = kEffectNodes.count, .live = kNodeLive}};
  for (std::size_t pool = 0; pool < kNodePools.size(); ++pool) {
    const NodePool &nodes = kNodePools[pool];
    tracks_[at++] = {.name = kNodePoolNames[pool],
                     .address = nodes.base,
                     .fields = kNodeFields,
                     .keys = kNodeKeys,
                     .records = {.stride = nodes.stride, .count = nodes.count, .live = kNodeLive}};
    tracks_[at++] = {.name = kNodeTransformNames[pool],
                     .address = nodes.base,
                     .fields = kNodeTransform,
                     .rule = &transform_,
                     .keys = kNodeKeys,
                     .records = {.stride = nodes.stride, .count = nodes.count, .live = kNodeLive}};
  }
  tracks_[at++] = {.name = "record transforms",
                   .address = RenderRecordPool::kRecords,
                   .fields = kRecordTransform,
                   .rule = &transform_,
                   .keys = kRecordKeys,
                   .records = recordRun};
  tracks_[at++] = {.name = "record angles",
                   .address = RenderRecordPool::kRecords,
                   .fields = kRecordAngles,
                   .keys = kRecordKeys,
                   .records = recordRun};
  tracks_[at++] = {
      .name = "effects", .address = kEffects, .fields = kEffectFields, .keys = kEffectKeys, .records = kEffectRun};
  tileGrid_ = &tracks_[at];
  tracks_[at++] = {.name = "tile grid",
                   .address = kTileGrid,
                   .fields = kTileGridFields,
                   .rule = &gridScroll_,
                   .records = {.live = kTileGridLive}};
  tracks_[at++] = {.name = "A01 sway", .address = kA01Sway, .fields = kAngleLow, .records = {.liveness = &a01_}};
  tracks_[at++] = {.name = "A02 scroll",
                   .address = kA02Scroll,
                   .fields = kWord,
                   .rule = &a02Scroll_,
                   .records = {.liveness = &a02_}};
  tracks_[at++] = {
      .name = "A02 band", .address = kA02Band, .fields = kWord, .rule = &a02Band_, .records = {.liveness = &a02_}};
  tracks_[at++] = {.name = "A06 scroll",
                   .address = kA06Scroll,
                   .fields = kWord,
                   .rule = &a06Scroll_,
                   .records = {.liveness = &a06_}};
  tracks_[at++] = {.name = "A0F scroll",
                   .address = kA0fScroll,
                   .fields = kWord,
                   .rule = &a0fScroll_,
                   .records = {.liveness = &a0f_}};
}

void TombaPan::blend(const Samples &samples, float t, std::span<std::uint32_t> out) const {
  out[0] = samples.to[0];
  out[1] = samples.to[1];
  if ((samples.to[1] & kTombaPanned) == 0) {
    return;
  }
  const std::uint32_t from = (samples.from[1] & kTombaPanned) != 0 ? samples.from[0] >> 16 : 0u;
  const std::uint32_t pan = psx::present::blendedWord(Motion::Angle12Low, from, samples.to[0] >> 16, t) & 0xFFFFu;
  out[0] = (samples.to[0] & 0xFFFFu) | pan << 16;
}

void ShakenView::jittered(const Units &eye, const Units &offset) {
  jitter_ = Jitter{.eye = eye, .offset = offset};
}

void ShakenView::built(Core &core) {
  Build build;
  for (std::size_t i = 0; i < kInputs; ++i) {
    build.inputs[i] = core.mem_r32(kLookInputs + 4u * static_cast<std::uint32_t>(i));
  }
  for (std::size_t i = 0; i < build.view.size(); ++i) {
    build.view[i] = core.mem_r32(kView + 4u * static_cast<std::uint32_t>(i));
  }
  Units eye{};
  for (std::size_t axis = 0; axis < eye.size(); ++axis) {
    eye[axis] = static_cast<std::int16_t>(build.inputs[axis] >> 16);
  }
  // Another step moved the eye since the tail did: the view holds no shake.
  if (jitter_ && jitter_->eye != eye) {
    jitter_.reset();
  }
  build.offset = jitter_ ? jitter_->offset : Units{};
  build_ = build;
}

void ShakenView::sample(Core &core) {
  core_ = &core;
  earlier_ = std::exchange(later_, std::nullopt);
  const std::optional<Build> build = std::exchange(build_, std::nullopt);
  if (!build) {
    return;
  }
  Tick tick{.build = *build};
  for (std::size_t i = 0; i < kRotationWords; ++i) {
    if (core.mem_r32(kView + 4u * static_cast<std::uint32_t>(i)) != build->view[i]) {
      lucent::debug("replay", "shaken view: the drawn view is not the camera's look-at");
      return;
    }
  }
  for (std::size_t axis = 0; axis < tick.after.size(); ++axis) {
    const std::uint32_t drawn = core.mem_r32(kViewTranslation + 4u * static_cast<std::uint32_t>(axis));
    tick.after[axis] = static_cast<std::int32_t>(drawn - build->view[kRotationWords + axis]);
  }
  for (std::size_t i = 0; i < kInputs; ++i) {
    tick.drawnInputs[i] = core.mem_r32(kLookInputs + 4u * static_cast<std::uint32_t>(i));
  }
  later_ = tick;
}

psx::present::MotionCount ShakenView::write(psx::present::GuestRedraw &redraw, float t) const {
  if (!later_) {
    return {};
  }
  if (!earlier_) {
    return {.drawn = 1, .unpaired = 1};
  }
  const Build &from = earlier_->build;
  const Build &to = later_->build;
  if (from.offset == Units{} && to.offset == Units{}) {
    return {.drawn = 1};
  }
  // The offset's whole units in the high half of an eye word.
  const auto shake = [](std::int16_t units) {
    return static_cast<std::uint32_t>(static_cast<std::uint16_t>(units)) << 16;
  };
  for (std::size_t i = 0; i < kInputs; ++i) {
    const bool eye = i < to.offset.size();
    const auto unshaken = [&](const Build &build) {
      return build.inputs[i] - (eye ? shake(build.offset[i]) : 0u);
    };
    const std::uint32_t word =
        psx::present::lerpWord(unshaken(from), unshaken(to), t) + (eye ? shake(to.offset[i]) : 0u);
    redraw.writeWord(kLookInputs + 4u * static_cast<std::uint32_t>(i), word);
  }
  const std::array lookAt{camera::kCameraObject};
  if (!redraw.run(camera::CutsceneCamera::kLookAt, lookAt)) {
    return {.drawn = 1, .unpaired = 1};
  }
  for (std::size_t axis = 0; axis < later_->after.size(); ++axis) {
    const std::uint32_t address = kViewTranslation + 4u * static_cast<std::uint32_t>(axis);
    const std::int32_t after = psx::present::lerpInt(earlier_->after[axis], later_->after[axis], t);
    redraw.writeWord(address, core_->mem_r32(address) + static_cast<std::uint32_t>(after));
  }
  for (std::size_t i = 0; i < kInputs; ++i) {
    redraw.writeWord(kLookInputs + 4u * static_cast<std::uint32_t>(i), later_->drawnInputs[i]);
  }
  return {.drawn = 1};
}

std::optional<std::array<float, 2>> DrawMotion::gridScrollPast(const psx::present::MotionSet &motion, float t) const {
  const std::optional<psx::present::MotionRule::Samples> samples = motion.pairedRecord(*tileGrid_, kTileGrid);
  if (!samples) {
    return std::nullopt;
  }
  std::array<float, 2> past{};
  for (std::size_t axis = 0; axis < past.size(); ++axis) {
    const auto half = axis == 0 ? psx::present::Halfword::Low : psx::present::Halfword::High;
    if (const std::optional<double> exact = gridScroll_.exactHalf(*samples, 0, half, t)) {
      past[axis] = static_cast<float>(*exact - std::nearbyint(*exact));
    }
  }
  return past;
}

} // namespace tomba
