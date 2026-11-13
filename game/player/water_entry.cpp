#include "player/water_entry.h"

#include "guest_abi.h"
#include "overrides/native_override_catalog.h"
#include "player/tomba_state.h"
#include "player/tomba_step.h"

#include <cstdint>

namespace tomba {
namespace {

using namespace tomba_field;

constexpr std::uint32_t kLanding = 0x800665F4u;
constexpr std::uint32_t kSeasideTick = 0x8010E408u;
constexpr std::uint32_t kLandingFrameSize = 0x18u;
constexpr std::uint32_t kSeasideFrameSize = 0x30u;

constexpr std::uint16_t kSwim = 0x100u;

constexpr std::uint32_t kArea = 0x800BF870u;
constexpr std::uint32_t kAreaEventFlags = 0x800BFE56u;
constexpr std::uint32_t kWaterSound = 0x800BF81Eu;
constexpr std::uint32_t kSeaEdgeOff = 0x800BF9DFu;
constexpr std::uint32_t kStandOnObject = 0x1F80027Bu;
constexpr std::uint32_t kSplashHeight = 0x1F80019Eu;
constexpr std::uint32_t kHeading = 0x1F800080u;

// Each water area's swim start (a0 Tomba).
constexpr std::uint32_t kSwimStartArea0 = 0x801140D0u;
constexpr std::uint32_t kSwimStartArea6 = 0x80110DD0u;
constexpr std::uint32_t kSwimStartArea8 = 0x8010F110u;
constexpr std::uint32_t kSwimStartArea14 = 0x8010A3DCu;

// Area 0's water-fall return points: the entry table at 0x800A4FF0 {x, y, z, entry}, and 0x80146EF8 for the sea
// edges.
constexpr std::uint32_t kReturnPoint = 0x800A4FF0u;
constexpr std::uint32_t kSeaEdgeReturn = 0x80146EF8u;
constexpr std::uint32_t kSplash = 0x8003116Cu; // (effect, position, -10)
constexpr std::uint32_t kSplashEffect = 0x107u;
constexpr std::uint32_t kSplashSound = 7u;
constexpr std::uint32_t kSpikeHit = 0x80023290u; // (Tomba): nonzero when the hit lands

constexpr std::uint8_t kSwimMode = 0x1Du;
constexpr std::uint8_t kCarrySwimMode = 0x32u; // from mode 6
constexpr std::uint8_t kSeaEdgeA = 0x1Du;
constexpr std::uint8_t kSeaEdgeB = 0x1Fu;
constexpr std::uint8_t kSpikeEntry = 0x1Cu;
constexpr std::uint32_t kEntry = 0x2Au;

bool waterFloor(std::uint16_t attribute) {
  const std::uint16_t type = attribute & 0xF00u;
  return type == 0x100u || type == 0x200u;
}

class Landing : public TombaStep {
public:
  explicit Landing(Core &core) : TombaStep(core, "WaterEntry::landing") {}

  std::uint32_t run() {
    const std::uint16_t attribute = u16(kFloorAttribute);
    if ((attribute & 0x8000u) == 0u || !waterFloor(attribute)) {
      return 0;
    }
    if (!has(kSwim)) {
      setU8(kMode, 4);
      setU8(kSubstate, 0xB);
      return 1;
    }
    const std::uint8_t area = core_.mem_r8(kArea);
    if (area == 0u) {
      call(kSwimStartArea0, tomba_);
    } else if (area == 6u) {
      if ((core_.mem_r16(kAreaEventFlags) & 0x40u) != 0u) {
        call(kSwimStartArea6, tomba_);
      } else {
        setU8(kMode, 4);
        setU8(kSubstate, 0xB);
      }
    } else if (area == 8u) {
      call(kSwimStartArea8, tomba_);
    } else if (area == 0xEu) {
      call(kSwimStartArea14, tomba_);
    }
    setU8(kApexLatch, 0);
    setU16(kGroundSpeed, 0);
    setU16(kDropVelocity, 0);
    setU8(kAirState, 0);
    setU8(0x148u, 0);
    return 1;
  }
};

class SeasideTick : public TombaStep {
public:
  explicit SeasideTick(Core &core) : TombaStep(core, "WaterEntry::seasideTick") {}

  void run() {
    if (core_.mem_r8(player::kGateBF80C) != 0u) {
      return;
    }
    const std::uint16_t attribute = u16(kFloorAttribute);
    if ((attribute & 0x8000u) != 0u) {
      waterFloorTick(attribute);
    } else if (u8(kEntry) == kSpikeEntry) {
      spikeTick();
    } else {
      seaEdgeTick();
    }
  }

private:
  // The splash and its sound, at Tomba's feet.
  void splash() {
    const std::uint32_t position = core_.r[psx::cpu::kSp] + 0x10u;
    core_.mem_w16(position + 2u, u16(kX + 2u));
    core_.mem_w16(position + 6u, static_cast<std::uint16_t>(u16(kY + 2u) + 0x82u));
    core_.mem_w16(position + 0xAu, u16(kZ + 2u));
    call(kSplash, kSplashEffect, position, static_cast<std::uint32_t>(-10));
    call(tomba_call::kSfxTrigger, kSplashSound, 0u, 0u);
  }
  void swimFromMode6() {
    setU8(kMode, kCarrySwimMode);
    setU8(0x149u, (u8(0x149u) & 1u) | 2u);
  }

  void waterFloorTick(std::uint16_t attribute) {
    core_.mem_w8(kWaterSound, 0);
    if (!waterFloor(attribute) || u8(kGroup) >= 2u) {
      return;
    }
    if (has(kSwim)) {
      if (u8(0x181u) != 0u || (u8(0x146u) == 4u && core_.mem_r8(kStandOnObject) != 0u)) {
        return;
      }
      setU16(kFloorAttribute, 0);
      if (u8(kMode) == 6u) {
        swimFromMode6();
      } else {
        setU8(kMode, kSwimMode);
        setU8(kSubstate, 0);
      }
      setU8(kGroup, 1);
      setU8(kPhase, 0);
      if (u8(kApexLatch) == 1u) {
        setU8(kApexLatch, 0);
      }
      return;
    }
    const std::uint32_t kind = (attribute >> 12u) & 7u;
    setU8(kGroup, 2);
    setU16(kFloorAttribute, 0);
    setU8(kSubstate, 0);
    setU8(kPhase, 0);
    if (kind < 2u || kind >= 6u) {
      return;
    }
    if (kind >= 4u) {
      setU8(0, 3);
      setU8(kMode, 4);
      return;
    }
    setU8(0, 5);
    const std::uint8_t entry = u8(kEntry);
    setU8(kSide, 0);
    setU8(kMode, 3);
    if (entry != kSeaEdgeA && entry != kSeaEdgeB) {
      setU16(0x7Au, core_.mem_r16(kReturnPoint));
      setU16(0x7Cu, core_.mem_r16(kReturnPoint + 2u));
      setU16(0x7Eu, core_.mem_r16(kReturnPoint + 4u));
      setU8(kEntry, core_.mem_r8(kReturnPoint + 6u));
      return;
    }
    const std::uint32_t point = kSeaEdgeReturn + (entry == kSeaEdgeA ? 0u : 6u);
    setU16(0x7Au, core_.mem_r16(point));
    setU16(0x7Cu, core_.mem_r16(point + 2u));
    setU16(0x7Eu, core_.mem_r16(point + 4u));
  }

  // Entry 0x1C: rising into the spike ring around (0x3CC0, 0x2CB2) hurts.
  void spikeTick() {
    const auto dx = static_cast<std::int16_t>(u16(kX + 2u) - 0x3CC0u);
    const auto dz = static_cast<std::int16_t>(u16(kZ + 2u) - 0x2CB2u);
    const std::uint32_t squared = static_cast<std::uint32_t>(dx * dx) + static_cast<std::uint32_t>(dz * dz);
    const std::int32_t distance = static_cast<std::int32_t>(result(player::kLeafIsqrt, squared) & 0xFFFFu);
    if (s16(0x80u) + 0x28 < distance) {
      return;
    }
    const std::int32_t top = (u16(0x84u) + u16(kY + 2u) + 0xBE6) & 0xFFFF;
    if (s16(0x86u) + 0xA0 < top || s16(kVelocityY) >= 0) {
      return;
    }
    const std::uint32_t heading =
        result(player::kLeafAtan2, static_cast<std::uint32_t>(-static_cast<std::int32_t>(dz)), static_cast<std::int32_t>(dx));
    core_.mem_w32(kHeading, heading);
    setU8(0x2Bu, static_cast<std::uint32_t>(static_cast<std::int32_t>(heading) >> 4));
    if (result(kSpikeHit, tomba_) != 0u) {
      call(tomba_call::kDamage, tomba_, 1u);
    }
  }

  // Entries 0x1D and 0x1F: stepping past the sea edge swims, or without the swim bit throws Tomba back.
  void seaEdgeTick() {
    const std::uint8_t entry = u8(kEntry);
    if (core_.mem_r8(kSeaEdgeOff) != 0u || u8(kGroup) >= 2u || u8(0x181u) != 0u || (u8(kAirState) & 1u) != 0u) {
      return;
    }
    if (entry != kSeaEdgeA && entry != kSeaEdgeB) {
      return;
    }
    if (s16(kY + 2u) < -0x7E3) {
      return;
    }
    setU16(kY + 2u, static_cast<std::uint16_t>(-0x7E4));
    if (entry == kSeaEdgeA) {
      if (s16(kX + 2u) < 0x413C) {
        setU16(kX + 2u, 0x41A0u);
      }
    } else if (s16(kZ + 2u) >= 0x2803) {
      setU16(kZ + 2u, 0x2802u);
    }
    core_.mem_w8(kWaterSound, 0);
    if (has(kSwim)) {
      setU16(kFloorAttribute, 0);
      if (u8(kMode) == 6u) {
        swimFromMode6();
        setU8(kPhase, 0);
      } else {
        setU8(kMode, kSwimMode);
        setU8(kSubstate, 0);
        setU8(kPhase, 0);
      }
      core_.mem_w16(kSplashHeight, u16(kY + 2u));
    } else {
      setU8(0, 7);
      setU8(kGroup, 2);
      setU8(kMode, 3);
      setU16(0x7Au, entry == kSeaEdgeA ? 0x4097u : 0x4F00u);
      setU16(0x7Cu, 0xF5BEu);
      setU8(kSide, 0);
      setU16(0x7Eu, entry == kSeaEdgeA ? 0x2BC0u : 0x2923u);
      setU8(kSubstate, 0);
      setU8(kPhase, 0);
    }
    splash();
  }
};

} // namespace

void WaterEntry::landing(Core *core) {
  psx::cpu::PreservedReturnAddress returnAddress(*core);
  psx::cpu::GuestFrameScope frame(*core, core->r[psx::cpu::kSp] - kLandingFrameSize);
  const std::uint32_t water = Landing(*core).run();
  core->r[2] = water;
}

void WaterEntry::seasideTick(Core *core) {
  psx::cpu::PreservedReturnAddress returnAddress(*core);
  psx::cpu::GuestFrameScope frame(*core, core->r[psx::cpu::kSp] - kSeasideFrameSize);
  SeasideTick(*core).run();
}

void WaterEntry::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declare(kLanding, "WaterEntry::landing", &WaterEntry::landing);
  natives.declareOverlay("A00", kSeasideTick, "WaterEntry::seasideTick", &WaterEntry::seasideTick);
}

} // namespace tomba
