// game/world/render_record_pool.cpp — the render-record free stack. See render_record_pool.h.
#include "render_record_pool.h"

#include "core.h"

namespace tomba2::world {
namespace {

// The halves the guest body builds the stack addresses from, left in a0 and a2.
constexpr std::uint32_t kCursorPage = 0x800E0000u;
constexpr std::uint32_t kCountPage = 0x800F0000u;

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

} // namespace tomba2::world
