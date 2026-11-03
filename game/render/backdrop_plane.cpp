// game/render/backdrop_plane.cpp — A0E FUN_80110CA4. See backdrop_plane.h.
#include "backdrop_plane.h"

#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "gte_registers.h"
#include "guest_abi.h"
#include "guest_call.h"
#include "guest_ordering_table.h"
#include "horizontal_visibility_cull.h"
#include "list_job.h"
#include "list_state_producer.h"
#include "object_draw.h"
#include "wide_window.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>

namespace tomba::render {
namespace {

using horizontal_cull::Domain;
using horizontal_cull::Visibility;

constexpr std::uint32_t kEntry = 0x80110CA4u;
constexpr GuestFrameSpill kSpills[] = {
    {31, 0x54}, {22, 0x50}, {21, 0x4C}, {20, 0x48}, {19, 0x44}, {18, 0x40}, {17, 0x3C}, {16, 0x38}};
constexpr psx::cpu::GuestCallSite kRotMatrix{.entry = 0x80085480u, .returnPc = 0x80110D04u};
constexpr psx::cpu::GuestCallSite kMulMatrix{.entry = 0x80084110u, .returnPc = 0x80110D1Cu};
constexpr psx::cpu::GuestCallSite kApplyMatrix{.entry = 0x80084220u, .returnPc = 0x80110D28u};
constexpr psx::cpu::GuestCallSite kSpriteHalf{.entry = 0x801104D0u, .returnPc = 0x801113F4u};

constexpr std::uint32_t kNodeAngles = 0x48u;
constexpr std::uint32_t kNodePosition = 0x2Cu;
constexpr std::uint32_t kView = 0x1F800000u; // MATRIX; translation at +0x14
constexpr std::uint32_t kCamera = 0x1F8000F8u;
constexpr std::uint32_t kCameraTranslation = 0x1F80010Cu;
constexpr std::uint32_t kDepthCue = 0x1F800090u;
constexpr std::uint32_t kClock = 0x1F80017Cu;      // u16; its bits 1-4 pick the texture frame
constexpr std::uint32_t kArenaPhase = 0x800BF850u; // below 2 before the boss rises

constexpr std::int16_t kCellWidth = 1200;
constexpr std::int16_t kCellHeight = 2400;
constexpr std::int16_t kLeftEdge = -6000;
constexpr std::int16_t kTopRow = -7200;
constexpr std::int16_t kBottomRow = 7200;
constexpr std::uint32_t kColumns = 10u;
constexpr std::uint32_t kQuadBytes = 0x34u;
constexpr std::uint32_t kQuadWords = 12u;
constexpr std::uint32_t kArenaBytes = (7u + 1u) * kColumns * kQuadBytes;
// The guest keeps a corner at x <= 320 and y <= 240, one past the screen.
constexpr int kKeptPastEdge = 1;
constexpr int kKeptHeight = horizontal_cull::kGuestFrameHeight + kKeptPastEdge;

constexpr std::uint8_t kPlaneCode = 0x3Cu; // GT4
constexpr std::uint8_t kHazeCode = 0x3Eu;  // GT4, semi-transparent
constexpr std::uint16_t kTexturePage = 0x2Du;
constexpr std::uint16_t kClut = 0x3F3Eu;
constexpr std::uint32_t kGrey = 0x808080u;
constexpr std::uint32_t kSeam = 0xDFDFDFu;
constexpr std::uint32_t kDeepWater = 0x201000u;
constexpr std::uint32_t kBlack = 0u;
constexpr std::uint8_t kTileSpan = 0x3Fu;
constexpr std::uint8_t kHalfTile = 0x20u;
constexpr std::uint8_t kTileHeight = 0x3Eu;

struct Corner {
  std::int16_t x;
  std::int16_t y;
  std::int16_t z;
};

struct Quad {
  std::array<Corner, 4> corners; // two top, two bottom
  std::array<std::uint32_t, 4> colours;
  std::array<std::uint8_t, 4> u;
  std::array<std::uint8_t, 4> v;
  std::uint8_t code;
};

std::uint32_t xy(const Corner &corner) {
  return static_cast<std::uint16_t>(corner.x) | (static_cast<std::uint32_t>(static_cast<std::uint16_t>(corner.y)) << 16);
}

// GT4 word offsets of corner i's screen point, colour, and uv.
constexpr std::array<std::uint32_t, 4> kScreen = {0x08u, 0x14u, 0x20u, 0x2Cu};
constexpr std::array<std::uint32_t, 4> kColour = {0x04u, 0x10u, 0x1Cu, 0x28u};
constexpr std::array<std::uint32_t, 4> kUv = {0x0Cu, 0x18u, 0x24u, 0x30u};

class Grids {
public:
  Grids(const psx::present::EmitMemory &memory, const OrderingTable &ot, const Visibility &visible)
      : mMemory(memory), mOt(ot), mVisible(visible), mPool(memory), mPacket(mPool.cursor()) {
    const std::uint32_t frame = (mMemory.mem_r16(kClock) >> 1) & 0xFFFFu;
    mU = static_cast<std::uint8_t>((frame & 3u) << 6);
    mV = static_cast<std::uint8_t>((frame << 4) & 0xC0u);
    mBias = mMemory.mem_r32(kArenaPhase) < 2u ? -0x50 : 0x78;
  }

  void draw() {
    for (std::int32_t row = kTopRow; row <= kBottomRow; row += kCellHeight) {
      bool odd = false;
      for (std::int32_t x = kLeftEdge; x < -kLeftEdge; x += kCellWidth, odd = !odd) {
        planeCell(row, x, odd);
      }
    }
    bool odd = false;
    for (std::int32_t x = kLeftEdge; x < -kLeftEdge; x += kCellWidth, odd = !odd) {
      hazeCell(x, odd);
    }
    mPool.setCursor(mPacket);
  }

private:
  std::array<std::uint8_t, 4> columnU(bool odd) const {
    const auto left = static_cast<std::uint8_t>(odd ? mU : mU + kHalfTile);
    const auto right = static_cast<std::uint8_t>(odd ? mU + kHalfTile : mU + kTileSpan);
    return {left, right, left, right};
  }

  // A plane cell whose bottom edge is model y `row`; below y 0 the texture is mirrored and darkened.
  void planeCell(std::int32_t row, std::int32_t x, bool odd) {
    const auto left = static_cast<std::int16_t>(x);
    const auto right = static_cast<std::int16_t>(x + kCellWidth);
    const auto top = static_cast<std::int16_t>(row - kCellHeight);
    const auto bottom = static_cast<std::int16_t>(row);
    const auto far = static_cast<std::uint8_t>(mV + kTileHeight);
    Quad quad{{{{left, top, 0}, {right, top, 0}, {left, bottom, 0}, {right, bottom, 0}}}, {}, columnU(odd), {}, kPlaneCode};
    if (row <= 0) {
      const std::uint32_t lower = row == 0 ? kSeam : kGrey;
      quad.colours = {kGrey, kGrey, lower, lower};
      quad.v = {mV, mV, far, far};
    } else {
      const std::uint32_t upper = row == kCellHeight ? kGrey : kDeepWater;
      quad.colours = {upper, upper, kDeepWater, kDeepWater};
      quad.v = {far, far, mV, mV};
    }
    if (!project(quad)) {
      return;
    }
    std::int32_t depth = static_cast<std::int32_t>(mMemory.core().gte().readData(psx::gte::kOtz)) + mBias;
    if (static_cast<std::uint32_t>(depth + 0x27) < 0x2Bu) {
      depth = 5;
    }
    std::int32_t bucket = OrderingTable::compressDepth(depth);
    if (bucket < OrderingTable::kNearestBucket) {
      return;
    }
    bucket = std::min(bucket, OrderingTable::kFarthestBucket);
    bucket += row <= 0 ? 0x1E : 0x28;
    if (bucket > 0x7FC) {
      bucket = 0x7FD;
    }
    finish(quad, static_cast<std::uint32_t>(bucket));
  }

  // A haze cell on y 0 from z 0 to 1200, grey at the seam and fading to black.
  void hazeCell(std::int32_t x, bool odd) {
    const auto left = static_cast<std::int16_t>(x);
    const auto right = static_cast<std::int16_t>(x + kCellWidth);
    const auto far = static_cast<std::uint8_t>(mV + kTileHeight);
    Quad quad{{{{left, 0, 0}, {right, 0, 0}, {left, 0, kCellWidth}, {right, 0, kCellWidth}}},
              {kGrey, kGrey, kBlack, kBlack},
              columnU(odd),
              {mV, mV, far, far},
              kHazeCode};
    if (!project(quad)) {
      return;
    }
    const std::int32_t bucket = OrderingTable::compressDepth(
        static_cast<std::int32_t>(mMemory.core().gte().readData(psx::gte::kOtz)) + mBias);
    if (!OrderingTable::inDepthRange(bucket)) {
      return;
    }
    finish(quad, static_cast<std::uint32_t>(bucket));
  }

  bool transformFailed() const {
    return static_cast<std::int32_t>(mMemory.core().gte().readControl(psx::gte::kFlag)) < 0;
  }

  // RTPT corners 0-2, RTPS corner 3, the screen gate, then AVSZ4 into OTZ.
  bool project(const Quad &quad) const {
    auto &gte = mMemory.core().gte();
    gte.writeData(psx::gte::kVxy0, xy(quad.corners[0]));
    gte.writeData(psx::gte::kVz0, static_cast<std::uint16_t>(quad.corners[0].z));
    gte.writeData(psx::gte::kVxy1, xy(quad.corners[1]));
    gte.writeData(psx::gte::kVz1, static_cast<std::uint16_t>(quad.corners[1].z));
    gte.writeData(psx::gte::kVxy2, xy(quad.corners[2]));
    gte.writeData(psx::gte::kVz2, static_cast<std::uint16_t>(quad.corners[2].z));
    gte.execute(psx::gte::kRtpt);
    if (transformFailed()) {
      return false;
    }
    for (std::uint32_t corner = 0; corner < 3u; ++corner) {
      mMemory.storeVertex(mPacket + kScreen[corner], gte.projected(corner));
    }
    gte.writeData(psx::gte::kVxy0, xy(quad.corners[3]));
    gte.writeData(psx::gte::kVz0, static_cast<std::uint16_t>(quad.corners[3].z));
    mMemory.mem_w16(mPacket + 0x1Au, kTexturePage);
    gte.execute(psx::gte::kRtps);
    if (transformFailed()) {
      return false;
    }
    mMemory.storeVertex(mPacket + kScreen[3], gte.projected(2));
    std::array<std::uint32_t, 4> xs{};
    std::array<std::uint32_t, 4> ys{};
    for (std::size_t corner = 0; corner < 4u; ++corner) {
      xs[corner] = mMemory.mem_r16(mPacket + kScreen[corner]);
      ys[corner] = mMemory.mem_r16(mPacket + kScreen[corner] + 2u);
    }
    if (!mVisible.keepsX(xs.data(), horizontal_cull::kQuadCorners, Domain::Packed16)) {
      return false;
    }
    bool keptY = false;
    for (const std::uint32_t y : ys) {
      keptY = keptY || horizontal_cull::insideAxis(y, Domain::Packed16, kKeptHeight);
    }
    if (!keptY) {
      return false;
    }
    mMemory.mem_w16(mPacket + 0xEu, kClut);
    gte.execute(psx::gte::kAvsz4);
    return true;
  }

  void finish(const Quad &quad, std::uint32_t bucket) {
    for (std::size_t corner = 0; corner < 4u; ++corner) {
      mMemory.mem_w8(mPacket + kUv[corner], quad.u[corner]);
      mMemory.mem_w8(mPacket + kUv[corner] + 1u, quad.v[corner]);
      mMemory.mem_w32(mPacket + kColour[corner], quad.colours[corner]);
    }
    mMemory.mem_w8(mPacket + 7u, quad.code);
    mOt.link({.packet = mPacket, .words = kQuadWords, .bucket = bucket});
    mPacket += kQuadBytes;
  }

  const psx::present::EmitMemory &mMemory;
  const OrderingTable &mOt;
  const Visibility &mVisible;
  PacketPool mPool;
  std::uint32_t mPacket;
  std::uint8_t mU = 0;
  std::uint8_t mV = 0;
  std::int32_t mBias = 0;
};

// The guest cull keeps x <= right edge, one column past the draw window.
Visibility visibilityOf(Core &core) {
  const wide_window::Window window = wide_window::drawWindow(&core);
  return Visibility({window.left, window.right + kKeptPastEdge});
}

void drawGrids(const psx::present::EmitMemory &memory, const ListCall &call, Core &core) {
  const OrderingTable ot(memory, call.ot);
  const Visibility visible = visibilityOf(core);
  Grids(memory, ot, visible).draw();
}

// The node's view into GTE CR0-7, the far colour cleared.
void loadView(Core *c, std::uint32_t node) {
  psx::cpu::callGuestNow(*c, __func__, kRotMatrix, node + kNodeAngles, kView);
  psx::cpu::callGuestNow(*c, __func__, kMulMatrix, kCamera, kView, kView);
  psx::cpu::callGuestNow(*c, __func__, kApplyMatrix, node + kNodePosition, kView + 0x14u);
  for (std::uint32_t axis = 0; axis < 3u; ++axis) {
    const std::uint32_t at = kView + 0x14u + axis * 4u;
    c->mem_w32(at, c->mem_r32(at) + c->mem_r32(kCameraTranslation + axis * 4u));
  }
  auto &gte = c->gte();
  for (std::uint32_t word = 0; word < psx::gte::kRotationWords + psx::gte::kTranslationWords; ++word) {
    gte.writeControl(psx::gte::kRotation + word, c->mem_r32(kView + word * 4u));
  }
  gte.writeControl(psx::gte::kFarRed, 0u);
  gte.writeControl(psx::gte::kFarGreen, 0u);
  gte.writeControl(psx::gte::kFarBlue, 0u);
  c->mem_w32(kDepthCue, 0u);
}

class BackdropPlaneRender final : public ListStateProducer {
public:
  using ListStateProducer::ListStateProducer;

protected:
  void emit(const psx::present::EmitMemory &memory, std::uint32_t, const ListCall &call) const override {
    drawGrids(memory, call, core());
  }
};

} // namespace

void BackdropPlane::draw(Core *c) {
  GuestFrame<0x58, 8> frame(c, kSpills);
  const std::uint32_t node = c->r[4];
  loadView(c, node);
  {
    const psx::present::EmitMemory memory(*c);
    const std::uint32_t ot = OrderingTable::active(memory).base();
    const ListCall call{.list = node, .ot = ot, .count = 0, .a3 = 0, .sp = c->r[psx::cpu::kSp], .a1 = ot};
    const EmitterObject object(*c, kEntry, OwnObject{node});
    ListJobWriter job(*c, 0u, call, kArenaBytes);
    job.input(*c, kClock, sizeof(std::uint16_t));
    job.input(*c, kArenaPhase, sizeof(std::uint32_t));
    drawGrids(memory, call, *c);
    job.save(*c);
  }
  psx::cpu::callGuestNow(*c, __func__, kSpriteHalf, node);
}

void BackdropPlane::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declareOverlay("A0E", kEntry, "&BackdropPlane::draw", &BackdropPlane::draw);
}

void BackdropPlane::registerStateRenders(psx::present::StateProducerTable &renders, Core &core) {
  renders.install(kEntry, std::make_unique<BackdropPlaneRender>(core));
}

} // namespace tomba::render
