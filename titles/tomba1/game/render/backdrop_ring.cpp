#include "backdrop_ring.h"

#include "core.h"
#include "guest_call.h"
#include "overlay_images.h"
#include "r3000.h"

namespace tomba1 {
namespace {

using psx::cpu::kA0;

// The 2D entity pool the per-frame pass 0x8003438C updates: ten 0x6C-byte slots, each run through the type table.
constexpr std::uint32_t kPool = 0x800A3D08u;
constexpr std::uint32_t kPoolSlots = 10u;
constexpr std::uint32_t kSlotBytes = 0x6Cu;
constexpr std::uint32_t kTypeTable = 0x8007D57Cu;
constexpr std::uint32_t kColumnUpdate = 0x8011C894u;

constexpr std::uint32_t kCameraX = 0x1F800176u;
constexpr std::uint32_t kCameraY = 0x1F800186u;
// 24.8 offsets the ring adds to the camera, in 16-column steps.
constexpr std::uint32_t kShakeX = 0x800A38DCu;
constexpr std::uint32_t kShakeY = 0x800A38D8u;

// A column's phase is its home less the camera, in quarter columns, modulo 512; 384..448 is not placed.
constexpr std::uint32_t kRingColumns = 0x200u;
constexpr std::uint32_t kHiddenFirst = 0x180u;
constexpr std::uint32_t kHiddenSpan = 0x41u;
constexpr std::uint32_t kWrapFrom = 0x1C1u;
// How far a column's parts reach from its x (signed byte offsets).
constexpr int kPartReach = 0x80;

constexpr std::uint32_t kFrameAnims = 0x8013405Cu;
constexpr std::uint32_t kFrameTable = 0x1F8002D4u;
constexpr std::uint32_t kPauseFlags = 0x8009C62Bu;
constexpr std::uint32_t kStageEvent = 0x8009BCDDu;
constexpr std::uint32_t kStageEventArmed = 0x8009BCA4u;

constexpr std::uint32_t kAnimateLeader = 0x8011CB48u;
constexpr std::uint32_t kFadeStep = 0x8004FB54u;
constexpr std::uint32_t kPlaySound = 0x8001FFE8u;
constexpr std::uint32_t kPushDrawList = 0x80018D40u;
constexpr std::uint32_t kRelease = 0x800188D0u;

std::int32_t s32(std::uint32_t value) {
  return static_cast<std::int32_t>(value);
}

std::uint32_t phase(Core &core, std::uint32_t entity) {
  const std::int32_t shake = (s32(core.mem_r32(kShakeX)) >> 8) * 16;
  const auto offset = static_cast<std::uint32_t>(s32(core.mem_r32(entity + 0x30u)) -
                                                 static_cast<std::int32_t>(core.mem_r16(kCameraX)) + shake);
  return (offset >> 2u) & (kRingColumns - 1u);
}

bool hidden(std::uint32_t ringPhase) {
  return ringPhase - kHiddenFirst < kHiddenSpan;
}

// The x the guest gives a placed phase: [0, 448] as is, [449, 511] one ring to the left.
std::int32_t placedX(std::uint32_t ringPhase) {
  const auto value = static_cast<std::int32_t>(ringPhase);
  return ringPhase < kWrapFrom ? value : value - static_cast<std::int32_t>(kRingColumns);
}

std::uint16_t ringY(Core &core, std::uint32_t entity) {
  const std::int32_t shake = (s32(core.mem_r32(kShakeY)) >> 8) * 16;
  const std::int32_t offset =
      s32(core.mem_r32(entity + 0x34u)) - static_cast<std::int32_t>(core.mem_r16(kCameraY)) - shake;
  return static_cast<std::uint16_t>(static_cast<std::int16_t>(static_cast<std::uint16_t>(offset)) >> 2);
}

// 0x8011CA78: a placed column takes its ring x and y and joins the draw list; a hidden one keeps its last values.
void place(Core &core, std::uint32_t entity) {
  const std::uint32_t ringPhase = phase(core, entity);
  if (hidden(ringPhase)) {
    return;
  }
  core.mem_w16(entity + 0x12u, static_cast<std::uint16_t>(placedX(ringPhase)));
  core.mem_w16(entity + 0x1Au, 0u);
  core.mem_w8(entity + 1u, 1u);
  core.mem_w16(entity + 0x16u, ringY(core, entity));
  psx::cpu::callGuestNow(core, "BackdropRing::place", kPushDrawList, entity);
}

// The leader column (e+3 set) animates the shared frames and runs the stage's fade-in: wait for the event, fade
// e+0x2E down by 0x10 a step, then count e+0x2C down.
void lead(Core &core, std::uint32_t entity) {
  psx::cpu::callGuestNow(core, "BackdropRing::lead", kAnimateLeader, entity);
  const std::uint8_t step = core.mem_r8(entity + 5u);
  if (step == 0u) {
    if (core.mem_r8(kStageEvent) != 0u) {
      return;
    }
    core.mem_w8(kStageEventArmed, 1u);
    core.mem_w16(entity + 0x2Cu, 0x17Cu);
    core.mem_w16(entity + 0x2Eu, 0x80u);
    core.mem_w8(entity + 5u, static_cast<std::uint8_t>(core.mem_r8(entity + 5u) + 1u));
    return;
  }
  if (step == 1u) {
    psx::cpu::callGuestNow(core,
                           "BackdropRing::lead",
                           kFadeStep,
                           static_cast<std::uint32_t>(static_cast<std::int16_t>(core.mem_r16(entity + 0x2Eu))));
    const auto fade = static_cast<std::uint16_t>((core.mem_r16(entity + 0x2Eu) - 0x10u) & 0xFFu);
    core.mem_w16(entity + 0x2Eu, fade);
    if (fade == 0u) {
      psx::cpu::callGuestNow(core, "BackdropRing::lead", kPlaySound, 0x82u);
      core.mem_w16(entity + 0x2Cu, 0x17Cu);
      core.mem_w8(entity + 5u, static_cast<std::uint8_t>(core.mem_r8(entity + 5u) + 1u));
    }
  } else if (step == 2u) {
    const std::uint16_t count = core.mem_r16(entity + 0x2Cu);
    core.mem_w16(entity + 0x2Cu, static_cast<std::uint16_t>(count - 1u));
    if (count == 0u) {
      core.mem_w16(entity + 0x2Eu, 0x80u);
      core.mem_w8(entity + 5u, static_cast<std::uint8_t>(core.mem_r8(entity + 5u) - 1u));
    }
  } else {
    return;
  }
  if (core.mem_r8(kStageEvent) == 3u) {
    core.mem_w8(entity + 5u, 3u);
  }
}

// 0x8011C894 (entity): state 0 records the home and frames, 1 places the column each frame, 2 and 3 release it.
void update(Core *core) {
  Core &c = *core;
  const std::uint32_t entity = c.r[kA0];
  switch (c.mem_r8(entity + 4u)) {
  case 0u:
    c.mem_w16(entity + 0x1Eu, 0xAu);
    c.mem_w8(entity + 0x0Du, 0u);
    c.mem_w32(entity + 0x24u, c.mem_r32(kFrameAnims + c.mem_r8(entity + 0x0Cu) * 4u));
    c.mem_w32(entity + 0x30u, static_cast<std::uint32_t>(static_cast<std::int16_t>(c.mem_r16(entity + 0x12u))));
    c.mem_w8(entity + 0x6Bu, 0u);
    c.mem_w16(entity + 0x2Eu, 0u);
    c.mem_w8(entity + 4u, 1u);
    c.mem_w32(entity + 0x34u, static_cast<std::uint32_t>(static_cast<std::int16_t>(c.mem_r16(entity + 0x16u))));
    c.mem_w16(entity + 0x20u, 0xE8u);
    c.mem_w16(entity + 0x22u, 0x3Cu);
    c.mem_w32(entity + 0x3Cu, c.mem_r32(kFrameTable));
    return;
  case 1u:
    if ((c.mem_r8(kPauseFlags) & 8u) == 0u && c.mem_r8(entity + 3u) != 0u) {
      lead(c, entity);
    }
    place(c, entity);
    return;
  case 2u:
    c.mem_w8(entity + 4u, 3u);
    return;
  case 3u:
    psx::cpu::callGuestNow(c, "BackdropRing::update", kRelease, entity);
    return;
  default:
    return;
  }
}

bool member(Core &core, std::uint32_t entity) {
  return entity >= kPool && entity < kPool + kPoolSlots * kSlotBytes && (entity - kPool) % kSlotBytes == 0u &&
         core.mem_r8(entity) != 0u && core.mem_r32(kTypeTable + core.mem_r8(entity + 2u) * 4u) == kColumnUpdate &&
         core.mem_r8(entity + 4u) == 1u;
}

// Every x congruent to `base` modulo the ring whose parts reach `columns`, except `base` itself when `skipBase`.
template <class Each>
void ringPositions(std::int32_t base, const psx::present::CullWindow &columns, bool skipBase, Each each) {
  const auto ring = static_cast<std::int32_t>(kRingColumns);
  const int first = columns.left - kPartReach;
  std::int32_t x = base;
  while (x - ring >= first) {
    x -= ring;
  }
  while (x < first) {
    x += ring;
  }
  for (; x < columns.right + kPartReach; x += ring) {
    if (!skipBase || x != base) {
      each(x);
    }
  }
}

} // namespace

bool BackdropRing::install(const InstallOverride &install, std::uint64_t content) {
  if (content != kAreaFourOverlay) {
    return true;
  }
  return install(kColumnUpdate, "BackdropRing::update", update);
}

void BackdropRing::drawBeyondPicture(Core &core,
                                     std::uint32_t entity,
                                     const psx::present::CullWindow &columns,
                                     const Draw &draw) {
  if (!member(core, entity)) {
    return;
  }
  // Positions are taken relative to where the guest placed this column, so the camera may move after the update.
  const std::int32_t guestX = static_cast<std::int16_t>(core.mem_r16(entity + 0x12u));
  const std::int32_t guestY = static_cast<std::int16_t>(core.mem_r16(entity + 0x16u));
  ringPositions(guestX, columns, true, [&](std::int32_t x) {
    draw(entity, {static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(guestY)});
  });
  std::uint32_t first = 0u;
  for (std::uint32_t slot = 0; slot < kPoolSlots && first == 0u; ++slot) {
    const std::uint32_t candidate = kPool + slot * kSlotBytes;
    if (member(core, candidate) && !hidden(phase(core, candidate))) {
      first = candidate;
    }
  }
  if (first != entity) {
    return;
  }
  const auto ownPhase = static_cast<std::int32_t>(phase(core, entity));
  const std::int32_t ownY = static_cast<std::int16_t>(ringY(core, entity));
  for (std::uint32_t slot = 0; slot < kPoolSlots; ++slot) {
    const std::uint32_t column = kPool + slot * kSlotBytes;
    if (!member(core, column) || !hidden(phase(core, column))) {
      continue;
    }
    const std::int32_t x = guestX + static_cast<std::int32_t>(phase(core, column)) - ownPhase;
    const std::int32_t y = guestY + static_cast<std::int16_t>(ringY(core, column)) - ownY;
    ringPositions(x, columns, false, [&](std::int32_t at) {
      draw(column, {static_cast<std::uint16_t>(at), static_cast<std::uint16_t>(y)});
    });
  }
}

} // namespace tomba1
