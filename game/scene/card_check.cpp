#include "scene/card_check.h"

#include "card_load_machine.h"
#include "core.h"
#include "core/overrides/native_override_catalog.h"

#include <cstdint>

namespace tomba::scene {
namespace {

constexpr std::uint32_t kCheckPage = 0x8018DC2Cu;
// The browser record (0x800BF498) a0 points at.
constexpr std::uint32_t kState = 0x00u;
constexpr std::uint32_t kStep = 0x01u;
constexpr std::uint32_t kSubStep = 0x02u;
constexpr std::uint32_t kPortFlags = 0x3Cu;
constexpr std::uint32_t kPortResult = 0x4Au; // one byte per port
constexpr std::uint32_t kStepFrames = 0x50u;
// A port result that has not settled: unchecked, or a card found whose read is pending.
constexpr std::uint8_t kUnchecked = 0xFCu;
constexpr std::uint8_t kPending = 0x01u;
constexpr std::uint8_t kSelectState = 1;
// The pad halfword (0x800E7E68) and the page answer sm+0x50 the browser driver FUN_8007BE18 reads.
constexpr std::uint32_t kPadWord = 0x800E7E68u;
constexpr std::uint16_t kCircle = 0x2000u;
constexpr std::uint16_t kTriangle = 0x1000u;
constexpr std::uint32_t kPageAnswer = 0x50u;
constexpr std::uint16_t kAnswerBack = 2;
constexpr std::uint16_t kAnswerCancel = 3;

bool settled(std::uint8_t result) {
  return result != kUnchecked && result != kPending;
}

} // namespace

void CardCheck::step(Core *core) {
  Core &c = *core;
  const std::uint32_t browser = c.r[4];
  switch (c.mem_r8(browser + kStep)) {
  case 0:
    c.mem_w8(browser + kStepFrames, 0);
    c.mem_w8(browser + kStep, 1);
    break;
  case 1:
    // Retail counts kStepFrames to 11 here with nothing to wait on.
    c.mem_w8(browser + kStep, 2);
    break;
  case 2:
    if (settled(c.mem_r8(browser + kPortResult)) && settled(c.mem_r8(browser + kPortResult + 1u))) {
      c.mem_w8(browser + kSubStep, 0);
      c.mem_w8(browser + kStep, 0);
      c.mem_w8(browser + kState, kSelectState);
    }
    break;
  default:
    break;
  }
  const std::uint16_t pad = c.mem_r16(kPadWord);
  const std::uint32_t sm = c.mem_r32(kStatePtr);
  if ((pad & kCircle) != 0) {
    c.mem_w16(sm + kPageAnswer, (c.mem_r8(browser + kPortFlags) & 1u) == 0 ? kAnswerBack : kAnswerCancel);
  } else if ((pad & kTriangle) != 0) {
    c.mem_w16(sm + kPageAnswer, kAnswerCancel);
  }
}

void CardCheck::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declareOverlay("CRD", kCheckPage, "CardCheck::step", step);
}

} // namespace tomba::scene
