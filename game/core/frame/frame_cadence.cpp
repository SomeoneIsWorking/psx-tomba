#include "frame/frame_cadence.h"

#include "core.h"

#include <cstdlib>
#include <lucent/log.h>

namespace tomba {

Core &FrameCadence::host() const {
  if (core == nullptr) {
    // Every other Engine-owned subsystem fails the same way when its back-pointer is missing
    // (see game_ctx.cpp). Silently skipping the publish would leave the guest's quota byte
    // holding whatever boot left there, which is exactly the invisible pacing fault this class
    // exists to prevent.
    lucent::error("frame-cadence", "used before its Core back-pointer was wired");
    std::abort();
  }
  return *core;
}

void FrameCadence::publish() {
  Core &c = host();
  c.mem_w8(kQuotaAddress, vblanksPerLogicFrame_);
  const std::uint8_t readBack = c.mem_r8(kQuotaAddress);
  if (readBack != vblanksPerLogicFrame_) {
    // The guest reads this byte with `lbu` at 0x80050CD0 and nothing else in the image writes
    // it, so a store that did not land means the port's decision is not the number the engine
    // will act on. Fail loudly here rather than pacing against a number nobody can see.
    lucent::error("frame-cadence", "quota publish did not land: wrote {} read {}", vblanksPerLogicFrame_, readBack);
  }
}

void FrameCadence::beginLogicFrame() {
  Core &c = host();
  c.mem_w16(kDwellCounterAddress, 0);
  ++logicFrames_;
  frameVblankBase_ = vblanksSpent_;
  accountedVblanks_ = vblanksSpent_;
}

std::uint16_t FrameCadence::consumeVblank() {
  // Deliberately does NOT write the counter. The guest's own incrementer (LAB_800506B4, vsync
  // callback slot 4) advances it, and in this product it runs through the port's own sequencer tick
  // — see the header. Reading it here and reporting what the guest left is the point: the port
  // observes the guest's cadence mechanism instead of standing in for it.
  ++vblanksSpent_;
  return host().mem_r16(kDwellCounterAddress);
}

void FrameCadence::endLogicFrame(std::uint32_t frame) {
  const std::uint16_t counter = dwellCounter();
  if (counter < vblanksPerLogicFrame_) {
    lucent::error("frame-cadence",
                  "f{} spent {} display field(s) but the guest's own vblank callback left its dwell "
                  "counter at {} against a quota of {} — the engine's gate would still be shut",
                  frame,
                  vblanksThisLogicFrame(),
                  counter,
                  vblanksPerLogicFrame_);
  }
  const std::uint32_t unaccounted = unaccountedVblanks();
  if (unaccounted != 0u) {
    lucent::error("frame-cadence",
                  "f{} spent {} display field(s) outside the per-field work, so the logic frame "
                  "claims time it did not account for",
                  frame,
                  unaccounted);
  }
}

bool FrameCadence::gateOpen() const {
  // The guest's own comparison: `sltu v0,v0,v1` at 0x80050CD8 on an lhu'd counter and an lbu'd
  // quota. Unsigned on both sides, and the counter is a u16 the incrementer lets wrap — the same
  // wrap the image has, not a saturating one this owner invented.
  Core &c = host();
  return c.mem_r16(kDwellCounterAddress) >= c.mem_r8(kQuotaAddress);
}

std::uint16_t FrameCadence::dwellCounter() const {
  return host().mem_r16(kDwellCounterAddress);
}

} // namespace tomba
