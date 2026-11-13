#include "painted_room.h"

#include "core.h"
#include "mesh_chunk.h"
#include "overlay_images.h"

#include <algorithm>
#include <array>
#include <climits>
#include <optional>

namespace tomba1 {
namespace {

using psx::present::HorizontalMargins;
using psx::present::PictureBeyondBuffer;

// Where the area overlays load.
constexpr std::uint32_t kOverlayBase = 0x800E7388u;
// The 2D pool the resident walker 0x8004DD64 draws: ten 0x6C-byte slots, dispatched on their kind (e+0xA).
constexpr std::uint32_t kPool = 0x800A3D08u;
constexpr std::uint32_t kPoolSlots = 10u;
constexpr std::uint32_t kSlotBytes = 0x6Cu;

// Kind 3: a scrolling block map. The overlay's positioner copies the map from block 0x800B0680[0] into a texture and
// its drawer, 0x800EBA58 while bit 1 is set and 0x800EA3A4 otherwise, emits the first two rows of a 12-byte table
// (x, y, u, v, w, h) at an origin read through e+0x40, less 0x20 when positive, while e+0x6B is set. Past the map's
// last column the copy runs on into the next block row.
constexpr std::uint8_t kTablePicture = 3u;
constexpr std::uint32_t kDrawerSelect = 0x800A539Au;
constexpr std::uint32_t kSelectedDrawer = 0x800EBA58u;
constexpr std::uint32_t kDefaultDrawer = 0x800EA3A4u;
constexpr std::uint32_t kTableRows = 2u;
constexpr std::uint32_t kTableRowBytes = 12u;
constexpr std::int32_t kOriginInset = 0x20;
constexpr std::uint32_t kMapWidthBlocks = 0x800A39DCu;
constexpr std::uint32_t kFirstBlock = 0x800B0680u;
constexpr std::int32_t kBlockColumns = 32;

// Kind 2: the resident 0x8004E244 emits 0x12-byte rows (tpage, x, y, u, v, w, h, ...) at (e+0x12, e+0x16) from the
// list 0x8007EBEC names by e+3, up to a 0x7FFF tpage.
constexpr std::uint8_t kListPicture = 2u;
constexpr std::uint32_t kPictureLists = 0x8007EBECu;
constexpr std::uint32_t kListRowBytes = 0x12u;
constexpr std::uint16_t kListEnd = 0x7FFFu;

struct PictureSource {
  std::uint64_t overlay = 0;
  std::uint8_t kind = 0;
  std::uint32_t drawer = 0;
  std::uint32_t table = 0;
};

constexpr std::array kSources{
    PictureSource{.overlay = kAreaTwoOverlay, .kind = kTablePicture, .drawer = kDefaultDrawer, .table = 0x800EDAECu},
    PictureSource{.overlay = kAreaFiveOverlay, .kind = kTablePicture, .drawer = kSelectedDrawer, .table = 0x800FDCB8u},
    PictureSource{.overlay = kAreaTwoRoomsOverlay, .kind = kListPicture},
    PictureSource{.overlay = kAreaFiveRoomsOverlay, .kind = kListPicture},
};

std::optional<PictureSource> activeSource(Core &core) {
  const auto image = core.imageCatalog().resolve(kOverlayBase);
  if (!image) {
    return std::nullopt;
  }
  const auto description = core.imageCatalog().describe(*image);
  if (!description) {
    return std::nullopt;
  }
  const std::uint32_t drawer = (core.mem_r8(kDrawerSelect) & 2u) != 0u ? kSelectedDrawer : kDefaultDrawer;
  for (const PictureSource &source : kSources) {
    if (source.overlay == description->contentIdentity && (source.kind != kTablePicture || source.drawer == drawer)) {
      return source;
    }
  }
  return std::nullopt;
}

// The picture the walker drew this frame: active, running (e+4 1), placed in the draw list (e+1), of the kind.
std::optional<std::uint32_t> pictureEntity(Core &core, std::uint8_t kind) {
  for (std::uint32_t slot = 0; slot < kPoolSlots; ++slot) {
    const std::uint32_t entity = kPool + slot * kSlotBytes;
    if (core.mem_r8(entity) != 0u && core.mem_r8(entity + 1u) != 0u && core.mem_r8(entity + 4u) == 1u &&
        core.mem_r8(entity + 0x0Au) == kind && (kind != kTablePicture || core.mem_r8(entity + 0x6Bu) != 0u)) {
      return entity;
    }
  }
  return std::nullopt;
}

struct Columns {
  std::int32_t left = INT_MAX;
  std::int32_t right = INT_MIN;

  void add(std::int32_t x, std::int32_t width) {
    left = std::min(left, x);
    right = std::max(right, x + width);
  }
};

std::int32_t s16(std::uint16_t value) {
  return static_cast<std::int16_t>(value);
}

Columns tableColumns(Core &core, std::uint32_t entity, const PictureSource &source) {
  std::int32_t origin = s16(core.mem_r16(core.mem_r32(entity + 0x40u) + 2u));
  if (origin > 0) {
    origin -= kOriginInset;
  }
  Columns columns;
  for (std::uint32_t row = source.table; row < source.table + kTableRows * kTableRowBytes; row += kTableRowBytes) {
    columns.add(origin + s16(core.mem_r16(row)), s16(core.mem_r16(row + 8u)));
  }
  const std::int32_t mapWidth = core.mem_r16(kMapWidthBlocks);
  if (mapWidth == 0) {
    return {};
  }
  const std::int32_t mapLeft = origin - (core.mem_r16(kFirstBlock) % mapWidth) * kBlockColumns;
  columns.left = std::max(columns.left, mapLeft);
  columns.right = std::min(columns.right, mapLeft + mapWidth * kBlockColumns);
  return columns;
}

Columns listColumns(Core &core, std::uint32_t entity) {
  const std::int32_t origin = s16(core.mem_r16(entity + 0x12u));
  Columns columns;
  for (std::uint32_t row = core.mem_r32(kPictureLists + core.mem_r8(entity + 3u) * 4u); core.mem_r16(row) != kListEnd;
       row += kListRowBytes) {
    columns.add(origin + s16(core.mem_r16(row + 2u)), s16(core.mem_r16(row + 8u)));
  }
  return columns;
}

} // namespace

PictureBeyondBuffer PaintedRoomProjection::pictureBeyondBuffer(Core &core) const {
  const auto source = activeSource(core);
  if (!source) {
    return {};
  }
  const auto entity = pictureEntity(core, source->kind);
  if (!entity) {
    return {};
  }
  const Columns columns =
      source->kind == kTablePicture ? tableColumns(core, *entity, *source) : listColumns(core, *entity);
  if (columns.left > columns.right) {
    return {};
  }
  PictureBeyondBuffer beyond;
  beyond.picture = HorizontalMargins{-columns.left, columns.right - MeshChunk::kGuestWidth};
  return beyond;
}

} // namespace tomba1
