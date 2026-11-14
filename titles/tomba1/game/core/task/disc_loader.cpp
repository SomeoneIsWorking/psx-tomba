#include "disc_loader.h"

#include "core.h"
#include "guest_task_slots.h"

namespace tomba1 {
namespace {

std::uint64_t progress(Core &core, std::uint32_t record) {
  return (static_cast<std::uint64_t>(core.mem_r32(record + DiscLoader::kStepOffset)) << 32u) |
         core.mem_r32(DiscLoader::kQueueHead);
}

} // namespace

void DiscLoader::finishAfterFlip(Core &core, GuestTaskSlots &slots) {
  const std::uint32_t record = taskRecordAddress(kSlot);
  while (slots.entry(kSlot) == kEntry) {
    const std::uint16_t state = core.mem_r16(record + kTaskRecordState);
    if (state == kTaskStateWaiting && core.mem_r16(record + kTaskRecordStep) == 1u) {
      // The sleep tick's wake (0x800173B0), the field the drive no longer needs.
      core.mem_w16(record + kTaskRecordStep, 0u);
      core.mem_w16(record + kTaskRecordState, kTaskStateRunnable);
    } else if (state != kTaskStateRunnable) {
      return;
    }
    const std::uint64_t before = progress(core, record);
    // A budget exit is the unpack still running; it resumes here rather than on a later field.
    if (slots.runSlot(core, kSlot) && progress(core, record) == before) {
      return;
    }
  }
}

} // namespace tomba1
