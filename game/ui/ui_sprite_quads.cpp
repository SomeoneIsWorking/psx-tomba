// game/ui/ui_sprite_quads.cpp — UiSprite::quads, the shared 2D sprite emitter FUN_8007E1B8.
//
// A definition names a run of 16-byte pieces; each becomes one POLY_FT4 staged in scratchpad 0x1F800004 and
// copied into the pool. The low nibble of the attribute flags picks the corner order: 1 mirrors X, 2 and 5 mirror
// Y, 3 both; a mirrored axis pulls its texture coordinates in by one texel (5 pushes V out by one). 4 and anything
// above 5 leave the staged positions as the previous quad left them.
#include "core.h"
#include "render/anchored_packets.h"
#include "render/guest_ordering_table.h"
#include "ui/ui_sprite.h"

namespace tomba {
namespace {

constexpr std::uint32_t kStage = 0x1F800000u;

// The staged POLY_FT4, by byte offset from kStage.
constexpr std::uint32_t kColor = 0x04u;
constexpr std::uint32_t kCode = 0x07u;
constexpr std::uint32_t kXy0 = 0x08u;
constexpr std::uint32_t kUv0 = 0x0Cu;
constexpr std::uint32_t kClut = 0x0Eu;
constexpr std::uint32_t kXy1 = 0x10u;
constexpr std::uint32_t kUv1 = 0x14u;
constexpr std::uint32_t kXy2 = 0x18u;
constexpr std::uint32_t kUv2 = 0x1Cu;
constexpr std::uint32_t kXy3 = 0x20u;
constexpr std::uint32_t kUv3 = 0x24u;
constexpr std::uint32_t kPacketWords = 9u;
constexpr std::uint32_t kPacketBytes = 4u + kPacketWords * 4u;

constexpr std::uint8_t kQuad = 0x2C;     // POLY_FT4
constexpr std::uint8_t kQuadSemi = 0x2E; // semi-transparent
constexpr std::uint8_t kRawTexture = 0x01;

// A piece: uv0 + CLUT, uv1 + texture page, uv2, uv3, its default size and its offset from the anchor.
constexpr std::uint32_t kPieceBytes = 16u;
constexpr std::uint32_t kPieceUv2 = 0x08u;
constexpr std::uint32_t kPieceWidth = 0x0Au;
constexpr std::uint32_t kPieceHeight = 0x0Bu;
constexpr std::uint32_t kPieceUv3 = 0x0Cu;
constexpr std::uint32_t kPieceDx = 0x0Eu;
constexpr std::uint32_t kPieceDy = 0x0Fu;

enum class Axis : std::uint8_t { U = 0, V = 1 }; // byte offset within a UV pair

enum class Corners : std::uint8_t { Plain = 0, MirrorX = 1, MirrorY = 2, MirrorXY = 3, Held = 4, MirrorYOut = 5 };

// A placement extent: positive is the size, zero takes the piece's, negative adds to the piece's.
std::uint32_t extent(std::int16_t placed, std::uint8_t pieceSize) {
  if (placed > 0) {
    return static_cast<std::uint16_t>(placed);
  }
  if (placed == 0) {
    return pieceSize;
  }
  return pieceSize + static_cast<std::uint16_t>(placed);
}

class Stage {
public:
  explicit Stage(Core &core) : core_(core) {}

  void corner(std::uint32_t xy, std::uint32_t x, std::uint32_t y) const {
    core_.mem_w16(kStage + xy, static_cast<std::uint16_t>(x));
    core_.mem_w16(kStage + xy + 2u, static_cast<std::uint16_t>(y));
  }
  // Moves the U or V byte of every texture coordinate by `step`.
  void nudge(Axis axis, int step) const {
    for (const std::uint32_t uv : {kUv0, kUv1, kUv2, kUv3}) {
      const std::uint32_t at = kStage + uv + static_cast<std::uint32_t>(axis);
      core_.mem_w8(at, static_cast<std::uint8_t>(core_.mem_r8(at) + step));
    }
  }
  std::uint32_t word(std::uint32_t offset) const {
    return core_.mem_r32(kStage + offset);
  }

private:
  Core &core_;
};

} // namespace

// FUN_8007E1B8(placement r4, indexPtr r5, defBase r6, attrs r7)
// ORACLE: guest 0x8007E1B8
void UiSprite::quads(Core *c) {
  Core &core = *c;
  const std::uint32_t placement = core.r[4];
  const std::uint32_t indexPtr = core.r[5];
  const std::uint32_t defBase = core.r[6];
  const std::uint32_t attrs = core.r[7];
  const Stage stage(core);

  const std::uint16_t attrWord = core.mem_r16(attrs + 2u);
  const std::uint8_t flags = core.mem_r8(attrs);
  const std::uint8_t shade = flags & 0xF0u;
  const bool semi = (attrWord & 0x8000u) != 0u;
  const bool clutOverride = (attrWord & 0x7FFFu) != 0u;
  core.mem_w8(attrs, flags & 0x0Fu);

  const std::uint32_t entry =
      defBase + static_cast<std::uint32_t>(static_cast<std::int16_t>(core.mem_r16(indexPtr)) * 4);
  std::int32_t count = static_cast<std::int16_t>(core.mem_r16(entry));
  std::uint32_t piece = defBase + static_cast<std::uint32_t>(static_cast<std::int16_t>(core.mem_r16(entry + 2u)));
  render::AnchoredPackets drawn(core.mem_r32(placement), 0u);

  do {
    core.mem_w32(kStage + kUv0, core.mem_r32(piece));
    core.mem_w32(kStage + kUv1, core.mem_r32(piece + 4u));
    core.mem_w16(kStage + kUv2, core.mem_r16(piece + kPieceUv2));
    core.mem_w16(kStage + kUv3, core.mem_r16(piece + kPieceUv3));
    core.mem_w8(kStage + kCode, semi ? kQuadSemi : kQuad);
    if (shade != 0u) {
      core.mem_w8(kStage + kColor + 2u, shade);
      core.mem_w8(kStage + kColor + 1u, shade);
      core.mem_w8(kStage + kColor, shade);
    } else {
      core.mem_w8(kStage + kCode, core.mem_r8(kStage + kCode) | kRawTexture);
    }
    if (clutOverride) {
      core.mem_w16(kStage + kClut, core.mem_r16(attrs + 2u));
    }

    const std::uint32_t width =
        extent(static_cast<std::int16_t>(core.mem_r16(placement + 4u)), core.mem_r8(piece + kPieceWidth));
    const std::uint32_t height =
        extent(static_cast<std::int16_t>(core.mem_r16(placement + 6u)), core.mem_r8(piece + kPieceHeight));
    const std::uint32_t x0 = core.mem_r16(placement) + static_cast<std::int8_t>(core.mem_r8(piece + kPieceDx));
    const std::uint32_t y0 = core.mem_r16(placement + 2u) + static_cast<std::int8_t>(core.mem_r8(piece + kPieceDy));
    const std::uint32_t x1 = (x0 & 0xFFFFu) + width;
    const std::uint32_t y1 = (y0 & 0xFFFFu) + height;
    switch (static_cast<Corners>(core.mem_r8(attrs))) {
    case Corners::Plain:
      stage.corner(kXy0, x0, y0);
      stage.corner(kXy1, x1, y0);
      stage.corner(kXy2, x0, y1);
      stage.corner(kXy3, x1, y1);
      break;
    case Corners::MirrorX:
      stage.corner(kXy0, x1, y0);
      stage.corner(kXy1, x0, y0);
      stage.corner(kXy2, x1, y1);
      stage.corner(kXy3, x0, y1);
      stage.nudge(Axis::U, -1);
      break;
    case Corners::MirrorY:
    case Corners::MirrorYOut:
      stage.corner(kXy0, x0, y1);
      stage.corner(kXy1, x1, y1);
      stage.corner(kXy2, x0, y0);
      stage.corner(kXy3, x1, y0);
      stage.nudge(Axis::V, static_cast<Corners>(core.mem_r8(attrs)) == Corners::MirrorY ? -1 : 1);
      break;
    case Corners::MirrorXY:
      stage.corner(kXy0, x1, y1);
      stage.corner(kXy1, x0, y1);
      stage.corner(kXy2, x1, y0);
      stage.corner(kXy3, x0, y0);
      stage.nudge(Axis::U, -1);
      stage.nudge(Axis::V, -1);
      break;
    case Corners::Held:
    default:
      break;
    }

    const std::uint32_t bucket = core.mem_r8(attrs + 1u);
    const render::PacketPool packets(core);
    const std::uint32_t packet = packets.cursor();
    render::OrderingTable::active(core).link({.packet = packet, .words = kPacketWords, .bucket = bucket});
    std::uint32_t command[kPacketWords];
    for (std::uint32_t word = 0; word < kPacketWords; ++word) {
      command[word] = stage.word(kColor + word * 4u);
      core.mem_w32(packet + 4u + word * 4u, command[word]);
    }
    packets.setCursor(packet + kPacketBytes);
    drawn.add(bucket, command);
    piece += kPieceBytes;
    count -= 1;
  } while (count != 0);

  core.r[2] = stage.word(kUv3);
  if (core.emission.isOpen()) {
    drawn.save(core);
  }
}

} // namespace tomba
