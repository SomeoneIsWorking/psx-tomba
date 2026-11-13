#include "player/cold_field.h"

#include "core.h"
#include "guest_abi.h"
#include "overrides/native_override_catalog.h"
#include "player/actor_tomba.h"
#include "player/tomba_step.h"

#include <cstdint>

namespace tomba {
namespace {

constexpr std::uint32_t kTick = 0x8010A854u;
constexpr std::uint32_t kIceCrust = 0x8012E454u;
constexpr std::uint32_t kTickFrameSize = 0x18u;
constexpr std::uint32_t kCrustFrameSize = 0x30u;

constexpr std::uint32_t kWaterMode = 0x800BF816u;
constexpr std::uint32_t kWaterPhase = 0x800BF817u;
constexpr std::uint32_t kWaterSurface = 0x800BF812u;
constexpr std::uint32_t kWaterClamp = 0x800BF8CDu; // 0xFF: no clamp of the water walk
constexpr std::uint32_t kEntry = 0x800BF871u;
constexpr std::uint32_t kGameOverLatch = 0x800BF80Du;
constexpr std::uint32_t kWaterSound = 0x800BF81Eu;
constexpr std::uint32_t kAreaEventFlags = 0x800BFE56u;
constexpr std::uint32_t kColdPowder = 0x800BFAD5u;
constexpr std::uint32_t kPaused = 0x1F800137u;
constexpr std::uint32_t kFreeSlots = 0x800E7E74u; // a stack of free actor slots, growing down
constexpr std::uint32_t kFreedCount = 0x800ED098u;

constexpr std::uint32_t kSurfaceSnap = 0x80022C78u;   // (Tomba)
constexpr std::uint32_t kCanTakeDamage = 0x80022D08u; // (Tomba, 0, 1, 1)
constexpr std::uint32_t kCrustLayer = 0x8012E340u;    // (crust, layer)
constexpr std::uint32_t kSfx = 0x80074590u;
constexpr std::uint32_t kPieceCount = 0x800270F8u;
constexpr std::uint32_t kAllocEffect = 0x8007B26Cu;
constexpr std::uint32_t kPieceInit = 0x80027058u; // (effect, model, piece)
constexpr std::uint32_t kActorFree = 0x8007A624u;
constexpr std::uint32_t kActorPlace = 0x80051844u;

constexpr std::int16_t kColdLine = 0x1928; // Z past which the field is cold
constexpr std::uint8_t kInCrust = 2u;      // G+0x177 bit 1
constexpr std::uint8_t kFreezeSubstate = 6u;
constexpr std::uint8_t kDeathFloorSubstate = 5u;
constexpr std::uint8_t kHazardGroup = 2u;
constexpr std::uint16_t kFullScale = 0x1000u;
constexpr std::uint16_t kScaleStep = 0x800u;
constexpr std::uint8_t kLayers = 3u;
constexpr std::int32_t kMaxPieces = 6;

class Tick : public TombaStep {
public:
  explicit Tick(Core &core) : TombaStep(core, "ColdField::tick") {}

  void run() {
    if (core_.mem_r8(kWaterMode) != 0u) {
      waterMode();
      return;
    }
    if (static_cast<std::uint8_t>(core_.mem_r8(kEntry) - 1u) < 3u || core_.mem_r8(kPaused) != 0u ||
        core_.mem_r8(kGameOverLatch) != 0u) {
      return;
    }
    if ((u16(tomba_field::kFloorAttribute) & 0x8000u) != 0u) {
      deathFloor();
      return;
    }
    if ((core_.mem_r16(kAreaEventFlags) & 0x20u) != 0u || has(SuitAbility::ColdImmunity) ||
        s16(tomba_field::kZ + 2u) <= kColdLine || (u8(0x177u) & kInCrust) != 0u) {
      return;
    }
    const bool frozenStill = core_.mem_r8(kColdPowder) != 0u || result(kCanTakeDamage, tomba_, 0u, 1u, 1u) == 0u;
    setU8(tomba_field::kPhase, frozenStill ? 1u : 0u);
    setU8(tomba_field::kGroup, kHazardGroup);
    setU8(tomba_field::kMode, kFreezeSubstate);
    setU8(tomba_field::kSubstate, 0);
    setU8(0, u8(0) | 8u);
  }

private:
  void waterMode() {
    if (u8(0) == 2u) {
      return;
    }
    const std::uint16_t surface = core_.mem_r16(kWaterSurface);
    const std::uint8_t phase = core_.mem_r8(kWaterPhase);
    if (phase != 0u) {
      if (phase != 2u) {
        return;
      }
      if (core_.mem_r8(kWaterClamp) != 0xFFu) {
        if (s16(tomba_field::kX + 2u) < 0x2D2C) {
          setU16(tomba_field::kX + 2u, 0x2D2Bu);
        }
        if (s16(tomba_field::kZ + 2u) >= 0x4B34) {
          setU16(tomba_field::kZ + 2u, 0x4B34u);
        }
      }
    }
    if ((u8(tomba_field::kAirState) & 1u) != 0u) {
      return;
    }
    const std::int32_t top = static_cast<std::int16_t>(surface) - s16(tomba_field::kLean);
    if (s16(tomba_field::kY + 2u) < top) {
      return;
    }
    setU16(tomba_field::kY + 2u, surface - u16(tomba_field::kLean));
    call(kSurfaceSnap, tomba_);
  }

  void deathFloor() {
    core_.mem_w8(kWaterSound, 0);
    if ((u16(tomba_field::kFloorAttribute) & 0xF00u) != 0x700u) {
      return;
    }
    setU8(0, 3);
    setU8(tomba_field::kGroup, kHazardGroup);
    setU16(tomba_field::kFloorAttribute, 0);
    setU8(tomba_field::kMode, kDeathFloorSubstate);
    setU8(tomba_field::kSubstate, 0);
    setU8(tomba_field::kPhase, 0);
  }
};

// The crust actor: +4 its state, +5 the state's step, +0x46 the layer count, +0xC0 the layer models (scale at +0x3A),
// +0x42 the growth frames.
class IceCrust : public TombaStep {
public:
  explicit IceCrust(Core &core) : TombaStep(core, "ColdField::iceCrust"), crust_(core.r[4]) {
    tomba_ = player::ActorTomba::G_ADDR;
  }

  void run() {
    switch (crustU8(4)) {
    case 0:
      arm();
      return;
    case 1:
      grow();
      break;
    case 2:
      breakOff();
      break;
    case 3:
      call(kActorFree, crust_);
      return;
    default:
      return;
    }
    follow();
  }

private:
  std::uint8_t crustU8(std::uint32_t field) const {
    return core_.mem_r8(crust_ + field);
  }
  void setCrustU8(std::uint32_t field, std::uint32_t value) {
    core_.mem_w8(crust_ + field, static_cast<std::uint8_t>(value));
  }
  void setCrustU16(std::uint32_t field, std::uint32_t value) {
    core_.mem_w16(crust_ + field, static_cast<std::uint16_t>(value));
  }
  std::uint32_t layerSlot() const {
    return crust_ + (static_cast<std::uint32_t>(crustU8(0x46)) - 1u) * 4u + 0xC0u;
  }
  std::uint32_t layer() const {
    return core_.mem_r32(layerSlot());
  }

  // The field reaches Tomba: in it, not paused, in the crust state and without the immunity.
  bool cold() {
    return s16(tomba_field::kZ + 2u) > kColdLine && core_.mem_r8(kPaused) == 0u && (u8(0x177u) & kInCrust) != 0u &&
           !has(SuitAbility::ColdImmunity);
  }

  void arm() {
    if (!cold()) {
      setCrustU8(4, 3);
      return;
    }
    setCrustU8(4, 1);
    setCrustU8(0xB, 0);
    setCrustU8(0xD, 0);
    call(kCrustLayer, crust_, 0u);
    setCrustU8(0x46, 1);
    setCrustU16(0xBC, kFullScale);
    setCrustU16(0xBA, kFullScale);
    setCrustU16(0xB8, kFullScale);
    setCrustU16(0x42, 0);
  }

  // Grows the newest layer's scale by 0x800 to full; each 2 x layers frames adds a layer, three at most.
  bool scaleIn() {
    const std::uint32_t model = layer();
    core_.mem_w16(model + 0x3Au, static_cast<std::uint16_t>(core_.mem_r16(model + 0x3Au) + kScaleStep));
    const std::uint32_t grown = layer();
    if (core_.mem_r16(grown + 0x3Au) < kFullScale) {
      return false;
    }
    core_.mem_w16(grown + 0x3Au, kFullScale);
    return true;
  }

  void grow() {
    const std::uint8_t step = crustU8(5);
    if (step == 1u) {
      if (scaleIn()) {
        setCrustU8(5, step + 1u);
      }
    } else if (step == 0u) {
      scaleIn();
      const auto frames = static_cast<std::int16_t>(core_.mem_r16(crust_ + 0x42u) + 1u);
      setCrustU16(0x42, static_cast<std::uint16_t>(frames));
      const std::uint8_t layers = crustU8(0x46);
      if (frames >= layers * 2) {
        core_.mem_w16(layer() + 0x3Au, kFullScale);
        call(kCrustLayer, crust_, static_cast<std::uint32_t>(crustU8(0x46)));
        const auto next = static_cast<std::uint8_t>(crustU8(0x46) + 1u);
        setCrustU8(0x46, next);
        if (next > kLayers) {
          setCrustU8(5, crustU8(5) + 1u);
        }
      }
    }
    if (!cold()) {
      setCrustU8(4, 2);
      setCrustU8(5, 0);
    }
  }

  // Step 0 shatters the newest layer into pieces and frees its model; step 1 waits three frames between layers.
  void breakOff() {
    const std::uint8_t step = crustU8(5);
    if (step == 1u) {
      const auto wait = static_cast<std::uint16_t>(core_.mem_r16(crust_ + 0x40u) - 1u);
      setCrustU16(0x40, wait);
      if (wait == 0u) {
        setCrustU8(5, crustU8(5) - 1u);
      }
      return;
    }
    if (step != 0u) {
      return;
    }
    call(kSfx, 0x2Du, 0xBu, static_cast<std::uint32_t>(-0x32));
    const std::uint32_t slot = layerSlot();
    const auto pieces = static_cast<std::int32_t>(result(kPieceCount, core_.mem_r32(core_.mem_r32(slot) + 0x40u)));
    for (std::int32_t piece = 0; piece < pieces && piece < kMaxPieces; ++piece) {
      const std::uint32_t effect = result(kAllocEffect);
      if (effect == 0u) {
        continue;
      }
      const std::uint32_t model = core_.mem_r32(slot);
      core_.mem_w8(effect + 1u, 1);
      core_.mem_w16(effect + 0xCu, 0x800);
      core_.mem_w8(effect + 2u, static_cast<std::uint8_t>(piece));
      core_.mem_w16(effect + 0x1Eu, core_.mem_r16(model + 0x2Cu));
      core_.mem_w16(effect + 0x22u, core_.mem_r16(core_.mem_r32(slot) + 0x30u));
      core_.mem_w16(effect + 0x26u, core_.mem_r16(core_.mem_r32(slot) + 0x34u));
      call(kPieceInit, effect, core_.mem_r32(core_.mem_r32(slot) + 0x40u), static_cast<std::uint32_t>(piece));
    }
    core_.mem_w16(kFreedCount, static_cast<std::uint16_t>(core_.mem_r16(kFreedCount) + 1u));
    const std::uint32_t top = core_.mem_r32(kFreeSlots);
    core_.mem_w32(kFreeSlots, top - 4u);
    core_.mem_w32(top - 4u, core_.mem_r32(slot));
    setCrustU8(9, crustU8(9) - 1u);
    const auto layers = static_cast<std::uint8_t>(crustU8(0x46) - 1u);
    setCrustU8(0x46, layers);
    if (layers == 0u) {
      setCrustU8(4, 3);
      return;
    }
    setCrustU16(0x40, 3);
    setCrustU8(5, crustU8(5) + 1u);
  }

  // The crust sits on Tomba: his position and rotation, then his visibility.
  void follow() {
    for (const std::uint32_t field : {0x2Eu, 0x32u, 0x36u, 0x54u, 0x56u, 0x58u}) {
      setCrustU16(field, u16(field));
    }
    call(kActorPlace, crust_);
    setCrustU8(1, u8(1));
  }

  std::uint32_t crust_;
};

} // namespace

void ColdField::tick(Core *core) {
  psx::cpu::PreservedReturnAddress returnAddress(*core);
  psx::cpu::GuestFrameScope frame(*core, core->r[psx::cpu::kSp] - kTickFrameSize);
  Tick(*core).run();
}

void ColdField::iceCrust(Core *core) {
  psx::cpu::PreservedReturnAddress returnAddress(*core);
  psx::cpu::GuestFrameScope frame(*core, core->r[psx::cpu::kSp] - kCrustFrameSize);
  IceCrust(*core).run();
}

void ColdField::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declareOverlay("A05", kTick, "ColdField::tick", &ColdField::tick);
  natives.declareOverlay("A05", kIceCrust, "ColdField::iceCrust", &ColdField::iceCrust);
}

} // namespace tomba
