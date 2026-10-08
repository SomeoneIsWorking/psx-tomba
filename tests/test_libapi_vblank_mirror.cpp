#include "core.h"
#include "core/hle/libapi_intr.h"
#include "timing.h"

#include <cstdint>
#include <cstdio>
#include <memory>

namespace {

inline constexpr std::uint32_t kVblankCount = 0x800ABDE0u;
inline constexpr std::uint32_t kCallbackTable = 0x800ABDC0u;
inline constexpr std::uint32_t kUntouchedWord = 0xAFB3002Cu;
inline constexpr std::uint32_t kCallbackSlots = 8u;

int failures = 0;

void check(bool condition, const char *detail) {
  if (!condition) {
    std::printf("FAIL: %s\n", detail);
    ++failures;
  }
}

} // namespace

int main() {
  auto core = std::make_unique<Core>();
  auto unrelated = std::make_unique<Core>();
  psx::frame::Timing timing;
  core->r[29] = 0x801FFFE0u;
  core->r[31] = 0x80010000u;
  core->mem_w32(kVblankCount, kUntouchedWord);
  unrelated->mem_w32(kVblankCount, kUntouchedWord);
  for (std::uint32_t slot = 0; slot < kCallbackSlots; ++slot) {
    core->mem_w32(kCallbackTable + slot * sizeof(std::uint32_t), 0u);
  }

  timing.frameTick();
  check(timing.vblank == 1u, "shared host field count did not advance");
  check(core->mem_r32(kVblankCount) == kUntouchedWord && unrelated->mem_r32(kVblankCount) == kUntouchedWord,
        "shared field tick wrote a title guest address");

  tomba::LibapiIntr::mirrorHostVblank(*core, timing.vblank);
  check(core->mem_r32(kVblankCount) == 1u, "Tomba frame boundary did not mirror the host field count");
  check(unrelated->mem_r32(kVblankCount) == kUntouchedWord, "Tomba mirror wrote another Core's RAM");

  tomba::LibapiIntr::runVblankCallbacks(core.get());
  check(core->mem_r32(kVblankCount) == 2u, "Tomba VBlank handler did not apply its second write");

  timing.frameTick();
  tomba::LibapiIntr::mirrorHostVblank(*core, timing.vblank);
  check(core->mem_r32(kVblankCount) == 2u, "next Tomba frame did not preserve the measured write order");
  check(unrelated->mem_r32(kVblankCount) == kUntouchedWord, "unrelated Core inherited Tomba's mirror");

  std::printf("Tomba libapi VBlank mirror: %s\n", failures == 0 ? "PASS" : "FAIL");
  return failures == 0 ? 0 : 1;
}
