#include "debug/dev_debris.h"

#include "camera/camera_state.h"
#include "core.h"
#include "debug/dev_warp.h"
#include "guest_abi.h"
#include "guest_call.h"

#include <lucent/log.h>

namespace tomba {
namespace {

constexpr std::uint32_t kBurst = 0x80027144u; // (model, place, scale, count | type << 4)
constexpr std::uint32_t kTomba = camera::kCamGlobal;
constexpr std::uint32_t kFirstRecord = 0xC0u; // node: render record pointers
constexpr std::uint32_t kRecordModel = 0x40u;
constexpr std::uint32_t kPlace = 0x10u; // the place buffer in the frame: s16 x +2, y +6, z +0xA
constexpr std::uint32_t kFullScale = 0x800u;
constexpr std::uint32_t kPickupBurst = 0x18u; // 8 pieces of type 1, as the pickup throws them
constexpr GuestFrameSpill kSpills[] = {{31, 0x28}};

} // namespace

std::string DevDebris::arm() {
  armed_ = true;
  return "ok: debris burst armed";
}

void DevDebris::applyArmed(Core &core, std::uint32_t frame) {
  if (!armed_) {
    return;
  }
  armed_ = false;
  if (DevWarp::phase(core) != DevWarp::Phase::Running) {
    lucent::error("debris", "debris burst dropped at f{}: the field is not running", frame);
    return;
  }
  const GuestFrame<0x30, 1> guestFrame(&core, kSpills);
  const std::uint32_t place = core.r[29] + kPlace;
  core.mem_w16(place + 2u, core.mem_r16(kTomba + 0x2Eu));
  core.mem_w16(place + 6u, core.mem_r16(kTomba + 0x32u));
  core.mem_w16(place + 0xAu, core.mem_r16(kTomba + 0x36u));
  const std::uint32_t model = core.mem_r32(core.mem_r32(kTomba + kFirstRecord) + kRecordModel);
  psx::cpu::callGuestNow(core, __func__, kBurst, model, place, kFullScale, kPickupBurst);
  lucent::info("debris", "debris burst thrown at f{}", frame);
}

} // namespace tomba
