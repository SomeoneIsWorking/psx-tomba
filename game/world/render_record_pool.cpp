// game/world/render_record_pool.cpp — the render-record free stack. See render_record_pool.h.
#include "render_record_pool.h"

#include "core.h"
#include "ordering_table.h"

#include <lucent/log.h>

#include <cstdlib>
#include <cstring>

namespace tomba::world {
namespace {

// The halves the guest body builds the stack addresses from, left in a0 and a2.
constexpr std::uint32_t kCursorPage = 0x800E0000u;
constexpr std::uint32_t kCountPage = 0x800F0000u;

template <typename Word> Word ramWord(const Core &core, std::uint32_t address) {
  Word word = 0;
  std::memcpy(&word, core.ram + psx::gpu::mainRamOffsetOf(address), sizeof(word));
  return word;
}

} // namespace

std::uint32_t RenderRecordPool::allocate(Core &core) {
  const auto count = static_cast<std::int16_t>(core.mem_r16(kFreeCount));
  if (count <= 0) {
    return 0;
  }
  const std::uint32_t cursor = core.mem_r32(kFreeCursor);
  core.mem_w16(kFreeCount, static_cast<std::uint16_t>(count - 1));
  const std::uint32_t record = core.mem_r32(cursor);
  core.mem_w32(kFreeCursor, cursor + 4u);
  ++generations_[record & kRecordOffsetMask];
  return record;
}

void RenderRecordPool::allocateForGuest(Core &core) {
  const std::uint32_t count = core.mem_r16(kFreeCount);
  core.r[2] = allocate(core);
  core.r[4] = kCursorPage;
  core.r[5] = count;
  core.r[6] = kCountPage;
  if (static_cast<std::int16_t>(count) > 0) {
    core.r[3] = core.mem_r32(kFreeCursor);
  }
}

std::uint32_t RenderRecordPool::object(std::uint32_t record) const {
  const auto found = generations_.find(record & kRecordOffsetMask);
  return incarnationObject(record, found == generations_.end() ? 0u : found->second);
}

void RenderRecordPool::sample(const Core &core) {
  free_.reset();
  const auto count = ramWord<std::int16_t>(core, kFreeCount);
  const auto cursor = ramWord<std::uint32_t>(core, kFreeCursor);
  if (count <= 0) {
    return;
  }
  if (static_cast<std::uint32_t>(count) > kRecordCount ||
      cursor + 4u * static_cast<std::uint32_t>(count) != kFreeStackEnd) {
    lucent::error("records", "free stack cursor {:08X} count {} does not end at {:08X}", cursor, count, kFreeStackEnd);
    std::abort();
  }
  for (std::uint32_t entry = cursor; entry < kFreeStackEnd; entry += 4u) {
    const std::uint32_t offset =
        psx::gpu::mainRamOffsetOf(ramWord<std::uint32_t>(core, entry)) - psx::gpu::mainRamOffsetOf(kRecords);
    if (offset % kRecordStride != 0 || offset / kRecordStride >= kRecordCount) {
      lucent::error(
          "records", "free stack entry {:08X} names {:08X}, not a record", entry, ramWord<std::uint32_t>(core, entry));
      std::abort();
    }
    free_.set(offset / kRecordStride);
  }
}

bool RenderRecordPool::live(std::uint32_t record) const {
  const std::uint32_t offset = psx::gpu::mainRamOffsetOf(record) - psx::gpu::mainRamOffsetOf(kRecords);
  return !free_.test(offset / kRecordStride);
}

} // namespace tomba::world
