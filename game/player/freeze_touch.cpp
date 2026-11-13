#include "player/freeze_touch.h"

#include "guest_abi.h"
#include "overrides/native_override_catalog.h"
#include "player/tomba_step.h"

#include <cstdint>

namespace tomba {
namespace {

using namespace tomba_field;

constexpr std::uint32_t kContacts = 0x8010F1CCu;
constexpr std::uint32_t kContactsFrameSize = 0x30u;

constexpr std::uint32_t kContactList = 0x1F80013Cu;
constexpr std::uint32_t kContactCount = 0x1F800144u;
constexpr std::uint32_t kContactsLeft = 0x1F800182u;
constexpr std::uint32_t kContactAngle = 0x1F80009Cu;
constexpr std::uint32_t kAirLock = 0x1F800137u;
constexpr std::uint32_t kColdPowder = 0x800BFAD5u;
constexpr std::uint32_t kEventFlags = 0x800BFA0Au;

constexpr std::uint16_t kNoFruitState = 0x200u;

// Guest contact handlers (Tomba, actor[, ...]).
constexpr std::uint32_t kSpanTouch = 0x8001F830u;   // nonzero on contact
constexpr std::uint32_t kBoxTouch = 0x80022F04u;    // nonzero on contact
constexpr std::uint32_t kKnockBack = 0x80022D08u;   // (Tomba, actor, a2, a3): nonzero when it took
constexpr std::uint32_t kGrabTouch = 0x8001F40Cu;   // (Tomba, actor, 1): side, or < 0
constexpr std::uint32_t kGrabResolve = 0x8001FF7Cu; // (Tomba, actor, side, mode)
constexpr std::uint32_t kPushTouch = 0x80020364u;   // (Tomba, actor, a2)
constexpr std::uint32_t kStepTouch = 0x8001F054u;   // 2 on a hit
constexpr std::uint32_t kLiftTouch = 0x80023548u;   // nonzero on contact
constexpr std::uint32_t kKind27Touch = 0x800235A0u;
constexpr std::uint32_t kKind8Touch = 0x800205CCu;
constexpr std::uint32_t kKind9Touch = 0x8010DEF0u; // the actors with +0xC == 9
constexpr std::uint32_t kKindBTouch = 0x8010AC8Cu;
constexpr std::uint32_t kKindDTouch = 0x8010B8A0u;
constexpr std::uint32_t kKind11Touch = 0x8010C320u;
constexpr std::uint32_t kKind1BTouch = 0x8010BA18u;
constexpr std::uint32_t kKind23Touch = 0x8010B2FCu; // (Tomba, actor, 1 for 0x23 else 0)
constexpr std::uint32_t kKind2ATouch = 0x8010BC80u;
constexpr std::uint32_t kKind33Touch = 0x8010B168u;
constexpr std::uint32_t kKind34Touch = 0x8010C8B4u; // side, or < 0

constexpr std::uint8_t kFreezeSubstate = 4u;
constexpr std::uint8_t kSlideMode = 0x13u;
constexpr std::uint8_t kGrabbedMode = 0x18u;

class Contacts : public TombaStep {
public:
  explicit Contacts(Core &core) : TombaStep(core, "FreezeTouch::contacts") {}

  void run() {
    std::uint32_t list = core_.mem_r32(kContactList);
    core_.mem_w8(kContactsLeft, core_.mem_r8(kContactCount));
    while (core_.mem_r8(kContactsLeft) != 0u) {
      actor_ = core_.mem_r32(list);
      core_.mem_w8(kContactsLeft, core_.mem_r8(kContactsLeft) - 1u);
      list += 4u;
      if ((actorU8(0) & 1u) != 0u) {
        touch();
      }
    }
  }

private:
  std::uint8_t actorU8(std::uint32_t field) const {
    return core_.mem_r8(actor_ + field);
  }
  void setActorU8(std::uint32_t field, std::uint32_t value) {
    core_.mem_w8(actor_ + field, static_cast<std::uint8_t>(value));
  }
  bool fruitState() const {
    return (u16(0x17Eu) & kNoFruitState) != 0u;
  }
  void faceContact() {
    setU8(0x2Bu, static_cast<std::uint32_t>(static_cast<std::int32_t>(core_.mem_r32(kContactAngle)) >> 4));
  }
  std::uint32_t touchedBy(std::uint32_t handler) {
    return result(handler, tomba_, actor_);
  }

  void touch() {
    if (actorU8(0xCu) == 9u) {
      if ((u16(0x17Eu) & 0x8200u) == 0u) {
        call(kKind9Touch, tomba_, actor_);
      }
      return;
    }
    switch (actorU8(2)) {
    case 0x08:
      call(kKind8Touch, tomba_, actor_);
      return;
    case 0x0B:
      call(kKindBTouch, tomba_, actor_);
      return;
    case 0x0D:
      call(kKindDTouch, tomba_, actor_);
      return;
    case 0x11:
      call(kKind11Touch, tomba_, actor_);
      return;
    case 0x1B:
      call(kKind1BTouch, tomba_, actor_);
      return;
    case 0x1C:
      grab();
      return;
    case 0x23:
      call(kKind23Touch, tomba_, actor_, 1u);
      return;
    case 0x24:
    case 0x25:
      call(kKind23Touch, tomba_, actor_, 0u);
      return;
    case 0x26:
    case 0x47:
    case 0x56:
    case 0xCF:
      call(kPushTouch, tomba_, actor_, 0u);
      return;
    case 0x27:
      call(kKind27Touch, tomba_, actor_);
      return;
    case 0x28:
      freeze();
      return;
    case 0x29:
      if (touchedBy(kLiftTouch) != 0u) {
        setActorU8(0x5Eu, 1);
      }
      return;
    case 0x2A:
      call(kKind2ATouch, tomba_, actor_);
      return;
    case 0x30:
      slide();
      return;
    case 0x31:
      if (!fruitState() && touchedBy(kBoxTouch) != 0u) {
        faceContact();
        call(kKnockBack, tomba_, actor_, 1u, 0u);
        setActorU8(0x2Bu, 1);
      }
      return;
    case 0x33:
      call(kKind33Touch, tomba_, actor_);
      return;
    case 0x34:
      kind34();
      return;
    case 0x43:
      kind43();
      return;
    case 0x4E:
      if (touchedBy(kStepTouch) == 2u && !fruitState()) {
        faceContact();
        call(kKnockBack, tomba_, actor_, 1u, 1u);
      }
      return;
    default:
      return;
    }
  }

  // Kind 0x1C: held by the actor, or grabbed by it (mode 0x18) when it carries him.
  void grab() {
    const std::uint32_t side = result(kGrabTouch, tomba_, actor_, 1u);
    if (static_cast<std::int32_t>(side) < 0) {
      return;
    }
    if (u8(0x164u) == 0xCu) {
      call(kGrabResolve, tomba_, actor_, side, 4u);
    } else if (actorU8(0x5Eu) != 1u || side != 3u || u8(0) != 1u) {
      call(kGrabResolve, tomba_, actor_, side, 1u);
    } else {
      if (s16(0x16Eu) == 0 || core_.mem_r8(kAirLock) != 0u || fruitState()) {
        return;
      }
      setU8(0, u8(0) | 2u);
      setActorU8(0, 2);
      setActorU8(0x5Eu, 2);
      setU8(kMode, kGrabbedMode);
      setU8(kSubstate, 0);
      setU8(kPhase, 0);
    }
    core_.mem_w8(kContactsLeft, 0);
  }

  // Kind 0x28: the ice monster's touch freezes Tomba in light clothes.
  void freeze() {
    if (touchedBy(kSpanTouch) == 0u) {
      return;
    }
    faceContact();
    if (result(kKnockBack, tomba_, 0u, 1u, 1u) == 0u || (u8(0x177u) & 2u) != 0u ||
        !lightlyDressed(SuitEffect::FreezeGuard)) {
      return;
    }
    setU8(kMode, 0);
    setU8(kSubstate, kFreezeSubstate);
    setU8(kPhase, 0);
    setU8(0, u8(0) | 8u);
  }

  // Kind 0x30: the slide's start, or its push while the slide is not armed.
  void slide() {
    if (fruitState()) {
      return;
    }
    if (actorU8(0x5Eu) == 0u) {
      call(kPushTouch, tomba_, actor_, 2u);
      return;
    }
    if (touchedBy(kBoxTouch) == 0u || u8(kGroup) != 1u || u8(kMode) == kSlideMode) {
      return;
    }
    setU8(kMode, kSlideMode);
    setU8(kSubstate, 0);
    setU8(kPhase, 0);
    faceContact();
  }

  void kind34() {
    const auto side = static_cast<std::int32_t>(result(kKind34Touch, tomba_, actor_));
    if (side < 0 || (core_.mem_r8(kEventFlags) & 0xFu) < 2u) {
      return;
    }
    if (side == 1) {
      setU16(0x142u, 0xFD00u);
    } else {
      setU16(0x142u, 0xFF00u);
      setU8(0x16Au, 10);
    }
  }

  void kind43() {
    if (fruitState() || touchedBy(kBoxTouch) == 0u) {
      return;
    }
    faceContact();
    const bool powder = core_.mem_r8(kColdPowder) != 0u;
    if ((actorU8(0) & 8u) != 0u) {
      if (!powder) {
        call(kKnockBack, tomba_, actor_, 1u, 1u);
      }
      return;
    }
    if (!powder) {
      call(kKnockBack, tomba_, actor_, 1u, 0u);
    }
    setActorU8(0, 2);
    setActorU8(4, 2);
    setActorU8(5, 2);
    setActorU8(6, 0);
  }

  std::uint32_t actor_ = 0;
};

} // namespace

void FreezeTouch::contacts(Core *core) {
  psx::cpu::PreservedReturnAddress returnAddress(*core);
  psx::cpu::GuestFrameScope frame(*core, core->r[psx::cpu::kSp] - kContactsFrameSize);
  Contacts(*core).run();
}

void FreezeTouch::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declareOverlay(
      "A04", kContacts, "FreezeTouch::contacts", &FreezeTouch::contacts, psx::cpu::NativeReturn::Void);
}

} // namespace tomba
