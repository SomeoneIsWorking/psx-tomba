#include "debug/dev_ap_popup.h"

#include "camera/camera_state.h"
#include "core.h"
#include "debug/dev_warp.h"
#include "guest_call.h"

#include <lucent/log.h>

namespace tomba {
namespace {

constexpr std::uint32_t kSpawn = 0x80071B44u; // (node, amount, source): source 0 rises over Tomba
constexpr std::uint32_t kTomba = camera::kCamGlobal;
constexpr std::uint32_t kOverTomba = 0u;

} // namespace

std::string DevApPopup::arm(std::uint32_t amount) {
  amount_ = amount;
  return lucent::format("ok: AP popup {} armed", amount);
}

void DevApPopup::applyArmed(Core &core, std::uint32_t frame) {
  if (amount_ == 0u) {
    return;
  }
  const std::uint32_t amount = amount_;
  amount_ = 0u;
  if (DevWarp::phase(core) != DevWarp::Phase::Running) {
    lucent::error("ap-popup", "AP popup dropped at f{}: the field is not running", frame);
    return;
  }
  psx::cpu::callGuestNow(core, __func__, kSpawn, kTomba, amount, kOverTomba);
  lucent::info("ap-popup", "AP popup {} spawned at f{}", amount, frame);
}

} // namespace tomba
