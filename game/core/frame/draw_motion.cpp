#include "frame/draw_motion.h"

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

constexpr std::array<const char *, kNodePools.size()> kNodePoolNames{
    "nodes 0xC4", "nodes 0xD0", "nodes 0x108", "nodes 0x140"};

} // namespace

DrawMotion::DrawMotion(world::RenderRecordPool &records) {
  // No byte marks a free record: FUN_8007AAE8 and FUN_8007ADDC only move the free stack.
  const MotionRecords recordRun{
      .stride = RenderRecordPool::kRecordStride, .count = RenderRecordPool::kRecordCount, .liveness = &records};
  std::size_t at = 0;
  tracks_[at++] = {.name = "view", .address = kView, .fields = kViewFields, .rule = &transform_};
  tracks_[at++] = {.name = "view angles", .address = kViewAngles, .fields = kViewAngleFields};
  tracks_[at++] = {.name = "tomba", .address = kTomba, .fields = kNodeFields};
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
}

} // namespace tomba
