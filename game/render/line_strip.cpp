// game/render/line_strip.cpp — LineStrip, extent [0x80117E84, 0x80118374) of A08. See line_strip.h.
#include "line_strip.h"

#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "entry/session.h"
#include "fx_sprite_publish.h"
#include "gte_registers.h"
#include "guest_abi.h"
#include "guest_ordering_table.h"
#include "libgpu_draw_mode.h"
#include "list_state_producer.h"

#include <cstdlib>
#include <lucent/log.h>
#include <memory>

namespace tomba::render {
namespace {

// Node fields.
constexpr std::uint32_t kWorldPoint = 0x2Cu; // VX | VY, then VZ
constexpr std::uint32_t kOtBias = 0x32u;     // s16
constexpr std::uint32_t kScale = 0x50u;      // s16, << 4 into the matrix diagonal
constexpr std::uint32_t kFadeSpan = 0x54u;   // s16 divisor of the fade

constexpr std::uint32_t kStrip = 0x80148668u;
constexpr std::uint32_t kWindows = 7;
constexpr std::uint32_t kWindowStride = 0x10u;
constexpr std::uint32_t kStripBytes = (kWindows - 1u) * kWindowStride + 0x18u;

// The model-view compose area and the scene camera's translation.
constexpr std::uint32_t kCompose = 0x1F800000u;
constexpr std::uint32_t kComposeTranslation = kCompose + 0x14u;
constexpr std::uint32_t kSceneRotation = 0x1F8000F8u;
constexpr std::uint32_t kSceneTranslation = 0x1F80010Cu;
// The three columns of a CR-packed matrix, as (row 0, row 1, row 2) byte offsets.
constexpr std::uint32_t kColumns[3][3] = {{0u, 6u, 0xCu}, {2u, 8u, 0xEu}, {4u, 0xAu, 0x10u}};

constexpr std::uint32_t kFadeFloor = 20u;
constexpr std::int32_t kFullGrey = 0x80;
constexpr std::uint32_t kSemiPolyLine = 0x4A000000u;
constexpr std::uint32_t kPolyLineEnd = 0x55555555u;
constexpr std::uint32_t kLineWords = 5u;
constexpr std::uint32_t kLineBytes = 0x18u;
constexpr std::uint32_t kModeWords = 2u;
constexpr std::uint32_t kModeBytes = 0xCu;
constexpr std::uint32_t kLineXy[3] = {8u, 0xCu, 0x10u};
constexpr std::uint32_t kFirstTexturePage = 0x35u;
constexpr std::uint32_t kSecondTexturePage = 0x55u;
constexpr std::uint32_t kSecondLineStep = 0x00010002u; // (x + 2, y + 1)
constexpr std::uint32_t kPacketBytes = 2u * (kLineBytes + kModeBytes);

// The frame's locals: the GTE flag and the bucket.
constexpr std::uint32_t kFrameBytes = 0x48u;
constexpr std::uint32_t kFlagLocal = 0x18u;
constexpr std::uint32_t kBucketLocal = 0x1Cu;
constexpr GuestFrameSpill kSpills[10] = {
    {23, 60}, {31 /*ra*/, 68}, {30, 64}, {22, 56}, {21, 52}, {20, 48}, {19, 44}, {18, 40}, {17, 36}, {16, 32}};

// The grey of the lines: the fade level in R, G and B under the semi-transparent polyline command.
std::uint32_t lineColour(const psx::present::EmitMemory &memory, std::uint32_t node) {
  const std::int32_t scale = memory.mem_r16s(node + kScale);
  const std::int32_t span = memory.mem_r16s(node + kFadeSpan);
  if (span == 0) {
    lucent::error("line-strip", "node {:#010x} fades over a span of 0, where the guest traps", node);
    std::abort();
  }
  const auto grey = static_cast<std::int32_t>(
      static_cast<std::int16_t>(kFullGrey - (scale - static_cast<std::int32_t>(kFadeFloor)) * kFullGrey / span));
  const auto bits = static_cast<std::uint32_t>(grey);
  return (bits << 16) | (bits << 8) | kSemiPolyLine | bits;
}

// The node's scale as a diagonal matrix, turned into the camera's frame, with the camera-relative world point.
void composeTransform(const psx::present::EmitMemory &memory, std::uint32_t node) {
  psx::cpu::Gte &gte = memory.core().gte();
  const auto diagonal = static_cast<std::uint16_t>(memory.mem_r16s(node + kScale) << 4);
  for (const std::uint32_t offset : {2u, 4u, 6u, 0xAu, 0xCu, 0xEu}) {
    memory.mem_w16(kCompose + offset, 0);
  }
  for (std::uint32_t axis = 3; axis-- > 0;) {
    memory.mem_w32(kComposeTranslation + axis * 4u, 0);
  }
  memory.mem_w16(kCompose, diagonal);
  memory.mem_w16(kCompose + 8u, diagonal);
  memory.mem_w16(kCompose + 0x10u, diagonal);
  for (std::uint32_t word = 0; word < psx::gte::kRotationWords; ++word) {
    gte.writeControl(psx::gte::kRotation + word, memory.mem_r32(kSceneRotation + word * 4u));
  }
  for (const auto &column : kColumns) {
    for (std::uint32_t row = 0; row < 3u; ++row) {
      gte.writeData(psx::gte::kIr1 + row, memory.mem_r16(kCompose + column[row]));
    }
    gte.execute(psx::gte::kMvmvaRtIr);
    for (std::uint32_t row = 0; row < 3u; ++row) {
      memory.mem_w16(kCompose + column[row], static_cast<std::uint16_t>(gte.readData(psx::gte::kIr1 + row)));
    }
  }
  gte.writeData(psx::gte::kVxy0, memory.mem_r32(node + kWorldPoint));
  gte.writeData(psx::gte::kVz0, memory.mem_r32(node + kWorldPoint + 4u));
  gte.execute(psx::gte::kMvmvaRtV0);
  for (std::uint32_t axis = 0; axis < 3u; ++axis) {
    memory.mem_w32(kComposeTranslation + axis * 4u, gte.readData(psx::gte::kMac1 + axis));
  }
  for (std::uint32_t axis = 0; axis < 3u; ++axis) {
    const std::uint32_t at = kComposeTranslation + axis * 4u;
    memory.mem_w32(at, memory.mem_r32(at) + memory.mem_r32(kSceneTranslation + axis * 4u));
  }
  for (std::uint32_t word = 0; word < psx::gte::kRotationWords + psx::gte::kTranslationWords; ++word) {
    gte.writeControl(psx::gte::kRotation + word, memory.mem_r32(kCompose + word * 4u));
  }
}

struct Line {
  std::uint32_t packet;
  std::uint32_t colour;
  std::uint32_t bucket;
  std::uint32_t texturePage;
};

// One polyline at the projected window, then its DR_MODE; returns the next packet.
std::uint32_t
drawLine(const psx::present::EmitMemory &memory, const Line &line, std::uint32_t step, const OrderingTable &ot) {
  psx::cpu::Gte &gte = memory.core().gte();
  for (std::uint32_t corner = 0; corner < 3u; ++corner) {
    const psx::gte::ProjectedVertex projected = gte.projected(corner);
    const std::uint32_t word = projected.word();
    const std::uint32_t moved =
        (((word & 0xFFFFu) + (step & 0xFFFFu)) & 0xFFFFu) | ((((word >> 16) + (step >> 16)) & 0xFFFFu) << 16);
    memory.storeVertex(line.packet + kLineXy[corner], step == 0 ? projected : projected.steppedTo(moved));
  }
  memory.mem_w32(line.packet + 4u, line.colour);
  memory.mem_w32(line.packet + 0x14u, kPolyLineEnd);
  ot.link({.packet = line.packet, .words = kLineWords, .bucket = line.bucket});
  const std::uint32_t mode = line.packet + kLineBytes;
  setDrawMode(memory, {.packet = mode, .drawOnDisplay = 0, .dither = 1, .texturePage = line.texturePage, .window = 0});
  ot.link({.packet = mode, .words = kModeWords, .bucket = line.bucket});
  return mode + kModeBytes;
}

void emitForGuest(Core *core) {
  const std::uint32_t node = core->r[4];
  const ListCall call{
      .list = node, .ot = core->mem_r32(OrderingTable::kBasePointer), .count = 0, .a3 = 0, .sp = core->r[29], .a1 = 0};
  ::GuestFrame<kFrameBytes, 10> frame(core, kSpills);
  const EmitterObject object(*core, LineStrip::kEmitter, OwnObject{eng(core).graphicsBind.records.object(node)});
  ListJobWriter job(*core, 0, call, kWindows * kPacketBytes);
  keepSceneCamera(job, *core);
  job.input(*core, node + kWorldPoint, 6, InputBlend::Lanes16);
  job.input(*core, node + kOtBias, 2);
  job.input(*core, node + kScale, 2, InputBlend::Lanes16);
  job.input(*core, node + kScale + 2u, kFadeSpan + 2u - kScale - 2u);
  job.input(*core, kStrip, kStripBytes);
  LineStrip::emit(*core, call);
  job.save(*core);
}

class LineStripProducer final : public ListStateProducer {
public:
  using ListStateProducer::ListStateProducer;

protected:
  void emit(const psx::present::EmitMemory &memory, std::uint32_t, const ListCall &call) const override {
    LineStrip::emit(memory, call);
  }
};

} // namespace

void LineStrip::emit(const psx::present::EmitMemory &memory, const ListCall &call) {
  const std::uint32_t node = call.list;
  const std::uint32_t colour = lineColour(memory, node);
  composeTransform(memory, node);
  psx::cpu::Gte &gte = memory.core().gte();
  const OrderingTable ot(memory, call.ot);
  const PacketPool pool(memory);
  const std::uint32_t frame = call.sp - kFrameBytes;
  std::uint32_t packet = pool.cursor();
  for (std::uint32_t window = 0; window < kWindows; ++window) {
    const std::uint32_t vectors = kStrip + window * kWindowStride;
    for (std::uint32_t reg = 0; reg < 6u; ++reg) {
      gte.writeData(psx::gte::kVxy0 + reg, memory.mem_r32(vectors + reg * 4u));
    }
    gte.execute(psx::gte::kRtpt);
    memory.mem_w32(frame + kFlagLocal, gte.readControl(psx::gte::kFlag));
    if (static_cast<std::int32_t>(memory.mem_r32(frame + kFlagLocal)) < 0) {
      continue;
    }
    gte.execute(psx::gte::kAvsz3);
    const auto depth = static_cast<std::int32_t>(gte.readData(psx::gte::kOtz)) + memory.mem_r16s(node + kOtBias);
    std::int32_t bucket = OrderingTable::compressDepth(depth);
    if (!OrderingTable::inDepthRange(bucket)) {
      bucket = OrderingTable::kNoBucket;
    }
    memory.mem_w32(frame + kBucketLocal, static_cast<std::uint32_t>(bucket));
    if (bucket <= 0) {
      continue;
    }
    const auto at = static_cast<std::uint32_t>(bucket);
    packet =
        drawLine(memory, {.packet = packet, .colour = colour, .bucket = at, .texturePage = kFirstTexturePage}, 0, ot);
    packet = drawLine(memory,
                      {.packet = packet, .colour = colour, .bucket = at, .texturePage = kSecondTexturePage},
                      kSecondLineStep,
                      ot);
  }
  pool.setCursor(packet);
}

void LineStrip::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declareOverlay("A08", kEmitter, "LineStrip::emit", &emitForGuest);
}

void LineStrip::registerStateRenders(psx::present::StateProducerTable &renders, Core &core) {
  renders.install(kEmitter, std::make_unique<LineStripProducer>(core));
}

} // namespace tomba::render
