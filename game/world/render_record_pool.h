// game/world/render_record_pool.h — the render-record free stack and which life of each record a key names.
//
// Objects draw through render records (node+0xC0+4i), popped from one LIFO stack by FUN_8007AAE8: count
// s16 at 0x800ED098, cursor at 0x800E7E74 walking an array of record pointers. Despawn (FUN_8007ADDC,
// Spawn::despawn) and the shrink paths (0x80058230; A00, A04, A05, A07, A0B and OPN inline copies) push
// records back, so the next spawn reuses the record just freed. Each pop begins a new incarnation, so a
// producer keyed by the record never pairs the old object's last frame with the new object's first.
#pragma once

#include <cstdint>
#include <unordered_map>

class Core;

namespace tomba2::world {

// The generation sits above the main-RAM offset, so it wraps after 2048 allocations of one record.
inline constexpr std::uint32_t kRecordOffsetBits = 21u;
inline constexpr std::uint32_t kRecordOffsetMask = (1u << kRecordOffsetBits) - 1u;

constexpr std::uint32_t incarnationObject(std::uint32_t record, std::uint32_t generation) {
  return (generation << kRecordOffsetBits) | (record & kRecordOffsetMask);
}

class RenderRecordPool {
public:
  static constexpr std::uint32_t kFreeCount = 0x800ED098u;  // s16
  static constexpr std::uint32_t kFreeCursor = 0x800E7E74u; // next free record pointer
  static constexpr std::uint32_t kAllocate = 0x8007AAE8u;

  // FUN_8007AAE8: the next free record, or 0 when the stack is empty.
  std::uint32_t allocate(Core &core);
  // FUN_8007AAE8 as guest code calls it: v0 and the scratch registers the guest body leaves.
  void allocateForGuest(Core &core);
  // The producer object for the record's current incarnation.
  std::uint32_t object(std::uint32_t record) const;

private:
  std::unordered_map<std::uint32_t, std::uint32_t> generations_; // main-RAM offset -> generation
};

} // namespace tomba2::world
