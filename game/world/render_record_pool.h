// game/world/render_record_pool.h — the render-record free stack.
//
// Objects draw through render records (node+0xC0+4i), popped from one LIFO stack by FUN_8007AAE8: count
// s16 at 0x800ED098, cursor at 0x800E7E74 walking an array of record pointers. Despawn (FUN_8007ADDC,
// Spawn::despawn) and the shrink paths (0x80058230; A00, A04, A05, A07, A0B and OPN inline copies) push
// records back, so the next spawn reuses the record just freed.
#pragma once

#include "motion_set.h"

#include <bitset>
#include <cstdint>

class Core;

namespace tomba::world {

class RenderRecordPool final : public psx::present::MotionLiveness {
public:
  static constexpr std::uint32_t kRecords = 0x800F2740u;
  static constexpr std::uint32_t kRecordStride = 0x44u;
  static constexpr std::uint32_t kRecordCount = 520u;
  static constexpr std::uint32_t kFreeCount = 0x800ED098u;  // s16
  static constexpr std::uint32_t kFreeCursor = 0x800E7E74u; // next free record pointer
  static constexpr std::uint32_t kFreeStackEnd = 0x800ED8C0u;
  static constexpr std::uint32_t kAllocate = 0x8007AAE8u;

  // FUN_8007AAE8: the next free record, or 0 when the stack is empty.
  std::uint32_t allocate(Core &core);
  // FUN_8007AAE8 as guest code calls it: v0 and the scratch registers the guest body leaves.
  void allocateForGuest(Core &core);

  // A record is live while no free-stack entry names it.
  void sample(Core &core) override;
  [[nodiscard]] bool live(std::uint32_t record) const override;

private:
  std::bitset<kRecordCount> free_;
};

} // namespace tomba::world
