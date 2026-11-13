#include "sprite_parts.h"

#include "core.h"
#include "emit_memory.h"
#include "gte_registers.h"
#include "mesh_chunk.h"
#include "packet_pool.h"
#include "r3000.h"

#include <array>
#include <cstdlib>
#include <lucent/log.h>

namespace tomba1 {
namespace {

using psx::cpu::kA0;
using psx::cpu::kV0;
using psx::present::CornerDomain;
using psx::present::ScreenCull;

// The anchor's model position (x, y, z halfwords) and the matrix it is projected with.
constexpr std::uint32_t kAnchorModel = 0x1F800060u;
constexpr std::uint32_t kAnchorMatrix = 0x1F8000C0u;
// Where the anchor's FLAG, SXY and quartered SZ are parked.
constexpr std::uint32_t kAnchorFlag = 0x1F80008Cu;
constexpr std::uint32_t kAnchorScreen = 0x1F800070u;
constexpr std::uint32_t kAnchorDepth = 0x1F800074u;
// The anchor depth is quartered while the stage word reads 0x10005 and the object at 0x1F8001D4 is not in mode 3.
constexpr std::uint32_t kStageWord = 0x8009BCC8u;
constexpr std::uint32_t kQuarterDepthStage = 0x10005u;
constexpr std::uint32_t kModeObject = 0x1F8001D4u;
// The scale of every part not scaled by its distance (0x80048DC8).
constexpr std::uint32_t kPartScale = 0x1F8002B4u;
// The depth-scaled parts' reference: a phase bit, and a row and a column the entity is measured from.
constexpr std::uint32_t kDepthPhase = 0x1F8001C8u;
constexpr std::uint32_t kDepthRow = 0x1F8000F6u;
constexpr std::uint32_t kDepthColumn = 0x1F8000EEu;
// The ordering table pointer; slots start 0x10 past it and are linked only within 0xCA0 bytes of it.
constexpr std::uint32_t kOrderingTable = 0x1F8001E0u;
constexpr std::uint32_t kFirstSlot = 0x10u;
constexpr std::uint32_t kOrderingTableBytes = 0xCA0u;

constexpr std::uint32_t kPacketBytes = 0x28u;
constexpr std::uint32_t kPacketTag = 0x09000000u;
constexpr std::uint8_t kRawTexturedQuad = 0x2Du;
constexpr std::uint8_t kSemiTransparent = 0x02u;
constexpr std::uint32_t kPartStride = 0x10u;

struct Anchor {
  std::uint32_t screen = 0;
};

// The entity's anchor through RTPS with the scratch matrix; false when there is no frame or the GTE flagged an error.
bool projectAnchor(Core &core, std::uint32_t entity, Anchor &anchor) {
  if (core.mem_r32(entity + 0x24u) == 0u) {
    return false;
  }
  core.mem_w16(kAnchorModel, core.mem_r16(entity + 0x12u));
  core.mem_w16(kAnchorModel + 2u, core.mem_r16(entity + 0x16u));
  const std::uint16_t z = core.mem_r16(entity + 0x1Au);
  const bool quartered =
      core.mem_r32(kStageWord) == kQuarterDepthStage && core.mem_r16(core.mem_r32(kModeObject) + 0x4Cu) != 3u;
  core.mem_w16(kAnchorModel + 4u, quartered ? static_cast<std::uint16_t>(static_cast<std::int16_t>(z) >> 2) : z);
  psx::cpu::Gte &gte = core.gte();
  for (std::uint32_t word = 0; word < psx::gte::kRotationWords; ++word) {
    gte.writeControl(psx::gte::kRotation + word, core.mem_r32(kAnchorMatrix + word * 4u));
  }
  for (std::uint32_t axis = 0; axis < psx::gte::kTranslationWords; ++axis) {
    gte.writeControl(psx::gte::kTranslationX + axis, core.mem_r32(kAnchorMatrix + 0x14u + axis * 4u));
  }
  gte.writeData(psx::gte::kVxy0, core.mem_r32(kAnchorModel));
  gte.writeData(psx::gte::kVz0, core.mem_r32(kAnchorModel + 4u));
  gte.execute(psx::gte::kRtps);
  const std::uint32_t flag = gte.readControl(psx::gte::kFlag);
  core.mem_w32(kAnchorFlag, flag);
  if (static_cast<std::int32_t>(flag) < 0) {
    return false;
  }
  const psx::gte::ProjectedVertex screen = gte.projected(2);
  psx::present::EmitMemory(core).storeVertex(kAnchorScreen, screen);
  core.mem_w32(kAnchorDepth, gte.readData(psx::gte::kSz3) >> 2u);
  anchor.screen = screen.word();
  return true;
}

// How a drawer reads its frame: the frame index signed or not, and whether it counts parts down in kPartsLeft.
struct FrameWalk {
  bool signedIndex = true;
  bool countInScratch = false;
};

constexpr FrameWalk kAlignedWalk{.signedIndex = true, .countInScratch = false};
constexpr FrameWalk kRotatedWalk{.signedIndex = false, .countInScratch = true};
constexpr FrameWalk kScreenWalk{.signedIndex = false, .countInScratch = false};
constexpr std::uint32_t kPartsLeft = 0x1F800078u;

// A frame: `count` parts of kPartStride bytes from `first`.
struct Frame {
  std::uint32_t first = 0;
  std::int32_t count = 0;
};

// The entity's current frame entry in its table: a part count, then the first part's offset from the table.
std::uint32_t frameEntry(Core &core, std::uint32_t entity, bool signedIndex) {
  const std::uint16_t raw = core.mem_r16(core.mem_r32(entity + 0x24u));
  const std::uint32_t index =
      signedIndex ? static_cast<std::uint32_t>(static_cast<std::int16_t>(raw)) : std::uint32_t{raw};
  return core.mem_r32(entity + 0x3Cu) + index * 4u;
}

std::uint32_t firstPart(Core &core, std::uint32_t entity, std::uint32_t entry) {
  return core.mem_r32(entity + 0x3Cu) + static_cast<std::uint32_t>(static_cast<std::int16_t>(core.mem_r16(entry + 2u)));
}

Frame currentFrame(Core &core, std::uint32_t entity, const FrameWalk &walk) {
  const std::uint32_t entry = frameEntry(core, entity, walk.signedIndex);
  const Frame frame{.first = firstPart(core, entity, entry), .count = static_cast<std::int16_t>(core.mem_r16(entry))};
  if (frame.count <= 0) {
    lucent::error("tomba1-sprite-parts",
                  "entity 0x{:08X} frame entry 0x{:08X} has {} parts; the guest loop would never end",
                  entity,
                  entry,
                  frame.count);
    std::abort();
  }
  return frame;
}

// A part's placement from the anchor: offset (dx, dy), then size (w, h), in wrapping guest arithmetic.
struct PartRect {
  std::uint32_t dx = 0;
  std::uint32_t dy = 0;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
};

// The part's own offset and size bytes, unscaled (0x80048914).
std::uint32_t sext8(std::uint8_t value) {
  return (std::uint32_t{value} ^ 0x80u) - 0x80u;
}

PartRect plainRect(Core &core, std::uint32_t part) {
  return {.dx = sext8(core.mem_r8(part + 0x0Eu)),
          .dy = sext8(core.mem_r8(part + 0x0Fu)),
          .width = core.mem_r8(part + 0x0Au),
          .height = core.mem_r8(part + 0x0Bu)};
}

// The same bytes times a 4.12 scale, offsets less `bias` and sizes plus it before the shift (0x80048DEC, 0x80049C00).
PartRect scaledRect(Core &core, std::uint32_t part, std::uint32_t scale, std::uint32_t bias) {
  const auto offset = [&](std::uint32_t at) {
    const std::uint32_t value = sext8(core.mem_r8(part + at));
    return (value * scale - bias) >> 12u;
  };
  const auto size = [&](std::uint32_t at) {
    return (core.mem_r8(part + at) * scale + bias) >> 12u;
  };
  return {.dx = offset(0x0Eu), .dy = offset(0x0Fu), .width = size(0x0Au), .height = size(0x0Bu)};
}

// 0x80048D44: a distance-scaled entity scales with its distance from the reference row or column, the rest by
// kPartScale. 0x80048BF0 takes kind 1 as distance-scaled, 0x80049994 any odd kind.
std::uint32_t partScale(Core &core, std::uint32_t entity, bool distanceScaled) {
  if (!distanceScaled) {
    return core.mem_r32(kPartScale);
  }
  const auto half = [&](std::uint32_t address) {
    return static_cast<std::int32_t>(static_cast<std::int16_t>(core.mem_r16(address)));
  };
  const std::int32_t distance = (core.mem_r16(kDepthPhase) & 1u) == 0u ? half(kDepthRow) - half(entity + 0x1Au)
                                                                       : half(entity + 0x12u) - half(kDepthColumn);
  const auto product =
      static_cast<std::int32_t>(core.mem_r32(entity + 0x64u) * static_cast<std::uint32_t>(distance * 7 + 0x1000));
  return static_cast<std::uint32_t>(product >> 12);
}

bool mirrored(Core &core, std::uint32_t entity) {
  return (core.mem_r16(entity + 0x2Eu) & 1u) != 0u;
}

// The four corners into the packet at the cursor, as the guest writes them whether or not the part is kept. Mirrored
// parts grow right of the anchor, the rest left.
void writeAlignedCorners(Core &core, std::uint32_t packet, bool flip, std::uint32_t anchor, const PartRect &rect) {
  const std::uint32_t left = flip ? anchor + rect.dx : anchor - rect.dx;
  const auto x0 = static_cast<std::uint16_t>(left);
  const auto y0 = static_cast<std::uint16_t>((anchor >> 16u) + rect.dy);
  const auto x1 = static_cast<std::uint16_t>(flip ? x0 + rect.width : x0 - rect.width);
  const auto y1 = static_cast<std::uint16_t>(y0 + rect.height);
  core.mem_w16(packet + 0x08u, x0);
  core.mem_w16(packet + 0x0Au, y0);
  core.mem_w16(packet + 0x10u, x1);
  core.mem_w16(packet + 0x12u, y0);
  core.mem_w16(packet + 0x18u, x0);
  core.mem_w16(packet + 0x1Au, y1);
  core.mem_w16(packet + 0x20u, x1);
  core.mem_w16(packet + 0x22u, y1);
}

// Per-angle 1.3.12 factors for the screen x and y of a rotated offset, 256 angles each.
constexpr std::uint32_t kAngleX = 0x8007DB88u;
constexpr std::uint32_t kAngleY = 0x8007D988u;

std::int32_t sext16(std::uint32_t value) {
  return static_cast<std::int16_t>(value);
}

// One side of a rotated part: its offset along the part's width, the angle it is turned by, and its packet corners.
struct RotatedSide {
  std::int32_t across = 0;
  std::uint32_t angle = 0;
  std::uint32_t top = 0;
  std::uint32_t bottom = 0;
};

// 0x80049318: each corner is the anchor plus its width offset turned by its side's angle and its height offset
// turned by that angle plus a quarter turn either way.
void writeRotatedCorners(
    Core &core, std::uint32_t packet, bool flip, std::uint32_t anchor, const PartRect &rect, std::uint32_t turn) {
  const std::uint32_t near = turn & 0xFFu;
  const std::uint32_t far = (turn + 0x80u) & 0xFFu;
  const std::int32_t dx = sext16(rect.dx);
  const std::int32_t dy = sext16(rect.dy);
  const std::int32_t width = sext16(rect.width);
  const std::int32_t height = sext16(rect.height);
  const std::int32_t reach = sext16(static_cast<std::uint32_t>(-(dx + width)));
  const RotatedSide farSide = flip ? RotatedSide{-dx, far, 0x08u, 0x18u} : RotatedSide{-reach, far, 0x10u, 0x20u};
  const RotatedSide nearSide =
      flip ? RotatedSide{dx + width, near, 0x10u, 0x20u} : RotatedSide{reach + width, near, 0x08u, 0x18u};
  struct Turn {
    std::uint32_t table = 0;
    std::uint32_t angle = 0;
  };
  const auto turned = [&](Turn turn, std::int32_t offset) {
    const auto factor = static_cast<std::int16_t>(core.mem_r16(turn.table + (turn.angle & 0xFFu) * 2u));
    return static_cast<std::uint32_t>(
        static_cast<std::int32_t>(static_cast<std::uint32_t>(offset) * static_cast<std::uint32_t>(factor)) >> 12);
  };
  // A corner `down` along the part's height from `side`, at packet offset `slot`, its height turned a `quarter` more.
  struct Corner {
    std::uint32_t slot = 0;
    std::int32_t down = 0;
    std::uint32_t quarter = 0;
  };
  const auto corner = [&](const RotatedSide &side, const Corner &at) {
    const std::uint32_t angle = side.angle + at.quarter;
    core.mem_w16(packet + at.slot,
                 static_cast<std::uint16_t>(anchor + turned({kAngleX, side.angle}, side.across) +
                                            turned({kAngleX, angle}, at.down)));
    core.mem_w16(packet + at.slot + 2u,
                 static_cast<std::uint16_t>((anchor >> 16u) + turned({kAngleY, side.angle}, side.across) +
                                            turned({kAngleY, angle}, at.down)));
  };
  corner(farSide, {.slot = farSide.top, .down = -dy, .quarter = 0xC0u});
  corner(nearSide, {.slot = nearSide.top, .down = -dy, .quarter = 0x40u});
  corner(farSide, {.slot = farSide.bottom, .down = dy + height, .quarter = 0x40u});
  corner(nearSide, {.slot = nearSide.bottom, .down = dy + height, .quarter = 0xC0u});
}

bool keeps(Core &core, std::uint32_t packet, const ScreenCull &cull) {
  std::array<std::uint32_t, 4> xs{};
  std::array<std::uint32_t, 4> ys{};
  for (std::uint32_t corner = 0; corner < 4u; ++corner) {
    xs[corner] = core.mem_r16(packet + 0x08u + corner * 8u);
    ys[corner] = core.mem_r16(packet + 0x0Au + corner * 8u);
  }
  return cull.keepsY(ys.data(), 4, CornerDomain::Packed16) && cull.keepsX(xs.data(), 4, CornerDomain::Packed16);
}

std::int32_t entityLayer(Core &core, std::uint32_t entity) {
  return static_cast<std::int32_t>(sext8(core.mem_r8(entity + 0x0Fu)));
}

// A projected entity's depth: its layer, plus the anchor's quartered SZ unless e+0xB is set.
std::int32_t projectedDepth(Core &core, std::uint32_t entity) {
  const std::int32_t layer = entityLayer(core, entity);
  return core.mem_r8(entity + 0x0Bu) == 0u ? layer + static_cast<std::int32_t>(core.mem_r32(kAnchorDepth)) : layer;
}

// A part's packet, the entity it belongs to and the part record it is textured from.
struct PartPacket {
  std::uint32_t packet = 0;
  std::uint32_t entity = 0;
  std::uint32_t part = 0;
};

// 0x80048A8C: the part's texture words, the entity's semi-transparency, tpage and clut, then the link at `depth`.
void completePart(Core &core, const PartPacket &placed, std::int32_t depth) {
  const auto [packet, entity, part] = placed;
  const std::uint8_t flags = core.mem_r8(entity + 0x0Du);
  const std::uint8_t command = kRawTexturedQuad;
  core.mem_w8(packet + 7u,
              (flags >> 7u) != 0u ? command | kSemiTransparent
                                  : command & static_cast<std::uint8_t>(~kSemiTransparent));
  core.mem_w32(packet + 0x0Cu, core.mem_r32(part));
  core.mem_w32(packet + 0x14u, core.mem_r32(part + 4u));
  core.mem_w16(packet + 0x1Cu, core.mem_r16(part + 8u));
  core.mem_w16(packet + 0x24u, core.mem_r16(part + 0x0Cu));
  core.mem_w16(packet + 0x16u, core.mem_r16(packet + 0x16u) + core.mem_r16(entity + 0x1Eu));
  if ((flags & 1u) != 0u) {
    core.mem_w16(packet + 0x0Eu, core.mem_r16(entity + 8u));
  }
  const std::int32_t offset = depth * 4 < 0 ? 0 : depth * 4;
  const std::uint32_t table = core.mem_r32(kOrderingTable);
  const std::uint32_t slot = table + kFirstSlot + static_cast<std::uint32_t>(offset);
  if (slot - table >= kOrderingTableBytes) {
    return;
  }
  const std::uint32_t next = core.mem_r32(slot);
  core.mem_w32(slot, packet);
  core.mem_w32(packet, next | kPacketTag);
  core.mem_w32(PacketPool::kCursor, core.mem_r32(PacketPool::kCursor) + kPacketBytes);
}

// Where a frame is drawn: its screen anchor and the depth its parts link at.
struct FrameAt {
  std::uint32_t anchor = 0;
  std::int32_t depth = 0;
};

// Every part of the entity's current frame around the screen anchor, its corners placed by
// `place(packet, flip, anchor, part)`.
template <class Place>
void drawFrame(Core &core, std::uint32_t entity, const FrameWalk &walk, const FrameAt &at, Place place) {
  const auto [anchor, depth] = at;
  const ScreenCull cull = MeshChunk::cullFor(core);
  const Frame frame = currentFrame(core, entity, walk);
  const bool flip = mirrored(core, entity);
  std::uint32_t part = frame.first;
  for (std::int32_t left = frame.count; left != 0; --left, part += kPartStride) {
    if (walk.countInScratch) {
      core.mem_w32(kPartsLeft, static_cast<std::uint32_t>(left));
    }
    const std::uint32_t packet = PacketPool::reserve(core, kPacketBytes);
    place(packet, flip, anchor, part);
    if (keeps(core, packet, cull)) {
      completePart(core, {.packet = packet, .entity = entity, .part = part}, depth);
    }
  }
  if (walk.countInScratch) {
    core.mem_w32(kPartsLeft, 0u);
  }
}

template <class Place> void drawParts(Core &core, std::uint32_t entity, const FrameWalk &walk, Place place) {
  Anchor anchor;
  if (!projectAnchor(core, entity, anchor)) {
    return;
  }
  drawFrame(core, entity, walk, {.anchor = anchor.screen, .depth = projectedDepth(core, entity)}, place);
}

// 0x800487A4 (entity)
void drawPlainParts(Core *core) {
  drawParts(*core,
            core->r[kA0],
            kAlignedWalk,
            [core](std::uint32_t packet, bool flip, std::uint32_t anchor, std::uint32_t part) {
              writeAlignedCorners(*core, packet, flip, anchor, plainRect(*core, part));
            });
}

// 0x80048BF0 (entity)
void drawScaledParts(Core *core) {
  const std::uint32_t entity = core->r[kA0];
  const std::uint32_t scale = partScale(*core, entity, core->mem_r8(entity + 0x0Au) == 1u);
  drawParts(*core,
            entity,
            kAlignedWalk,
            [core, scale](std::uint32_t packet, bool flip, std::uint32_t anchor, std::uint32_t part) {
              writeAlignedCorners(*core, packet, flip, anchor, scaledRect(*core, part, scale, 0x1000u));
            });
}

// 0x80049134 (entity)
void drawRotatedParts(Core *core) {
  const std::uint32_t entity = core->r[kA0];
  const std::uint32_t turn = core->mem_r32(entity + 0x8Cu);
  drawParts(*core,
            entity,
            kRotatedWalk,
            [core, turn](std::uint32_t packet, bool flip, std::uint32_t anchor, std::uint32_t part) {
              writeRotatedCorners(*core, packet, flip, anchor, plainRect(*core, part), turn);
            });
}

// 0x80049994 (entity)
void drawRotatedScaledParts(Core *core) {
  const std::uint32_t entity = core->r[kA0];
  const std::uint32_t scale = partScale(*core, entity, (core->mem_r8(entity + 0x0Au) & 1u) != 0u);
  const std::uint32_t turn = core->mem_r32(entity + 0x8Cu);
  drawParts(*core,
            entity,
            kRotatedWalk,
            [core, scale, turn](std::uint32_t packet, bool flip, std::uint32_t anchor, std::uint32_t part) {
              writeRotatedCorners(*core, packet, flip, anchor, scaledRect(*core, part, scale, 0x1800u), turn);
            });
}

// 0x8004BAF0 (entity): the plain parts around the entity's own (x, y) halfwords, already in screen space, linked at its
// layer plus its z halfword.
void drawScreenParts(Core *core) {
  const std::uint32_t entity = core->r[kA0];
  const std::uint32_t anchor = core->mem_r16(entity + 0x12u) | (std::uint32_t{core->mem_r16(entity + 0x16u)} << 16u);
  const std::int32_t depth = entityLayer(*core, entity) + static_cast<std::int16_t>(core->mem_r16(entity + 0x1Au));
  drawFrame(*core,
            entity,
            kScreenWalk,
            {.anchor = anchor, .depth = depth},
            [core](std::uint32_t packet, bool flip, std::uint32_t at, std::uint32_t part) {
              writeAlignedCorners(*core, packet, flip, at, plainRect(*core, part));
            });
}

// The model quad's view: the camera matrix 0x1F8000F8 copied to kQuadView, whose translation becomes the entity's
// position turned by the scratch matrix plus that matrix's translation.
constexpr std::uint32_t kCameraMatrix = 0x1F8000F8u;
constexpr std::uint32_t kQuadView = 0x1F800000u;
constexpr std::uint32_t kMatrixWords = 8u;
constexpr std::uint32_t kMatrixTranslation = 0x14u;
// Where the quad's SZ average is parked.
constexpr std::uint32_t kQuadDepth = 0x1F800070u;
// The entity's four model corners, (x, y, z, pad) halfwords each.
constexpr std::uint32_t kQuadCorners = 0xB4u;
constexpr std::uint32_t kCornerBytes = 8u;

// 0x8004AA70 (entity): one raw-textured quad, the entity's four model corners projected at its position and textured
// by its frame's first part, linked at its layer plus the corners' average SZ.
void drawModelQuad(Core *core) {
  Core &c = *core;
  const std::uint32_t entity = c.r[kA0];
  for (std::uint32_t word = 0; word < kMatrixWords; ++word) {
    c.mem_w32(kQuadView + word * 4u, c.mem_r32(kCameraMatrix + word * 4u));
  }
  c.mem_w16(kAnchorModel, c.mem_r16(entity + 0x12u));
  c.mem_w16(kAnchorModel + 2u, c.mem_r16(entity + 0x16u));
  c.mem_w16(kAnchorModel + 4u, c.mem_r16(entity + 0x1Au));
  psx::cpu::Gte &gte = c.gte();
  for (std::uint32_t word = 0; word < psx::gte::kRotationWords; ++word) {
    gte.writeControl(psx::gte::kRotation + word, c.mem_r32(kAnchorMatrix + word * 4u));
  }
  gte.writeData(psx::gte::kVxy0, c.mem_r32(kAnchorModel));
  gte.writeData(psx::gte::kVz0, c.mem_r32(kAnchorModel + 4u));
  gte.execute(psx::gte::kMvmvaRtV0);
  for (std::uint32_t axis = 0; axis < psx::gte::kTranslationWords; ++axis) {
    const std::uint32_t at = kQuadView + kMatrixTranslation + axis * 4u;
    c.mem_w32(at, gte.readData(psx::gte::kIr1 + axis) + c.mem_r32(kAnchorMatrix + kMatrixTranslation + axis * 4u));
    gte.writeControl(psx::gte::kTranslationX + axis, c.mem_r32(at));
  }
  const std::uint32_t packet = PacketPool::reserve(c, kPacketBytes);
  const std::uint32_t corners = entity + kQuadCorners;
  for (std::uint32_t corner = 0; corner < 3u; ++corner) {
    gte.writeData(psx::gte::kVxy0 + corner * 2u, c.mem_r32(corners + corner * kCornerBytes));
    gte.writeData(psx::gte::kVz0 + corner * 2u, c.mem_r32(corners + corner * kCornerBytes + 4u));
  }
  gte.execute(psx::gte::kRtpt);
  const psx::present::EmitMemory memory(c);
  for (std::uint32_t corner = 0; corner < 3u; ++corner) {
    memory.storeVertex(packet + 0x08u + corner * 8u, gte.projected(corner));
  }
  gte.writeData(psx::gte::kVxy0, c.mem_r32(corners + 3u * kCornerBytes));
  gte.writeData(psx::gte::kVz0, c.mem_r32(corners + 3u * kCornerBytes + 4u));
  gte.execute(psx::gte::kRtps);
  gte.execute(psx::gte::kAvsz4);
  c.mem_w32(kQuadDepth, gte.readData(psx::gte::kOtz));
  memory.storeVertex(packet + 0x20u, gte.projected(2));
  if (!keeps(c, packet, MeshChunk::cullFor(c))) {
    return;
  }
  const std::uint32_t part = firstPart(c, entity, frameEntry(c, entity, false));
  completePart(c,
               {.packet = packet, .entity = entity, .part = part},
               entityLayer(c, entity) + static_cast<std::int32_t>(c.mem_r32(kQuadDepth)));
}

// 0x80045AF8 (packet): the four-corner test the area overlays' quad drawers call before linking a packet.
void quadOnScreen(Core *core) {
  core->r[kV0] = keeps(*core, core->r[kA0], MeshChunk::cullFor(*core)) ? 1u : 0u;
}

} // namespace

bool installSpriteParts(const InstallOverride &install) {
  return install(0x80045AF8u, "SpriteParts::quadOnScreen", quadOnScreen) &&
         install(0x800487A4u, "SpriteParts::plain", drawPlainParts) &&
         install(0x80048BF0u, "SpriteParts::scaled", drawScaledParts) &&
         install(0x80049134u, "SpriteParts::rotated", drawRotatedParts) &&
         install(0x80049994u, "SpriteParts::rotatedScaled", drawRotatedScaledParts) &&
         install(0x8004AA70u, "SpriteParts::modelQuad", drawModelQuad) &&
         install(0x8004BAF0u, "SpriteParts::screen", drawScreenParts);
}

} // namespace tomba1
