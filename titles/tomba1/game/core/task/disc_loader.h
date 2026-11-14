// disc_loader.h — Tomba! 1's disc loader task runs to its end right after the flip that woke it.
#pragma once

#include <cstddef>
#include <cstdint>

class Core;

namespace tomba1 {

class GuestTaskSlots;

// The loader 0x80021340 (task slot 2, started by 0x80017154(2, 0x80021340)) takes one step per field: queue check,
// CdControl(Setloc), CdSync, CdRead, CdReadSync, the file's unpack, the queue advance. Each step ends in a
// one-field yield (0x800171D4(1)) that waits on the drive; psxport's drive is instant, so after the flip the loader
// runs its steps, budget exits included, until it exits (raising 0x1F8001CE) or a step leaves its state unchanged.
// It runs after the flip, not in the task pass: a flagged file unpacks into the packet pool 0x800B3188, over the
// packets the task that queued the load linked into the OT the flip draws.
class DiscLoader {
public:
  inline static constexpr std::size_t kSlot = 2u;
  inline static constexpr std::uint32_t kEntry = 0x80021340u;
  // The loader's own record words: +0x48 its step, +0x4A the unpack's sub-step.
  inline static constexpr std::uint32_t kStepOffset = 0x48u;
  // 0x1F8002A0: the load queue head, advanced by step 6 (0x80021D70 appends at the tail 0x1F80029C).
  inline static constexpr std::uint32_t kQueueHead = 0x1F8002A0u;

  // Once per frame, after the flip 0x80016940 and the sleep tick 0x800173B0.
  static void finishAfterFlip(Core &core, GuestTaskSlots &slots);
};

} // namespace tomba1
