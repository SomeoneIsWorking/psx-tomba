// game/render/sop_ground.cpp — FUN_80109FE0, the SOP intro's ground blocks.
#include "sop_ground.h"

#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "guest_abi.h"
#include "guest_call.h"
#include "guest_ordering_table.h"
#include "native_dispatch.h"

namespace {

constexpr std::uint32_t kEntry = 0x80109FE0u;
constexpr GuestFrameSpill kSpills[] = {{31, 0x24}, {20, 0x20}, {19, 0x1C}, {18, 0x18}, {17, 0x14}, {16, 0x10}};
constexpr std::uint32_t kListCount = 6u;
constexpr std::uint32_t kListBlocks = 0xCu;
constexpr std::uint32_t kListIndices = 0x10u;
constexpr std::uint32_t kSceneCamera = 0x1F8000F8u; // GTE RT11..TRZ, control registers 0-7
constexpr std::uint32_t kSceneCameraWords = 8u;
constexpr std::uint32_t kBlockRecords = 4u;
constexpr std::uint32_t kGt3 = 0x801099B4u;
constexpr std::uint32_t kGt4 = 0x80109C80u;
constexpr std::uint32_t kReturnAfterGt3 = 0x8010A0A0u;
constexpr std::uint32_t kReturnAfterGt4 = 0x8010A0B4u;

} // namespace

void SopGround::draw(Core *c) {
  GuestFrame<40, 6> frame(c, kSpills);
  const std::uint32_t list = c->r[4];
  const std::uint32_t count = c->mem_r8(list + kListCount);
  if (count == 0u) {
    return;
  }
  const std::uint32_t orderingTable = tomba2::render::OrderingTable::active(*c).base();
  const std::uint32_t blocks = c->mem_r32(list + kListBlocks);
  for (std::uint32_t reg = 0; reg < kSceneCameraWords; reg++) {
    gte_write_ctrl(reg, c->mem_r32(kSceneCamera + reg * 4u));
  }
  for (std::uint32_t slot = 0; slot < count; slot++) {
    const std::uint32_t block = blocks + c->mem_r16(list + kListIndices + slot * 2u) * 4u;
    const std::uint32_t counts = c->mem_r32(block);
    const auto drawn = c->emission.instance(block);
    c->r[4] = block + kBlockRecords;
    c->r[5] = orderingTable;
    c->r[6] = counts & 0xFFu;
    c->r[31] = kReturnAfterGt3;
    psx::cpu::dispatchGuestToReturn0(*c, kGt3, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
    c->r[4] = c->r[2];
    c->r[5] = orderingTable;
    c->r[6] = (counts >> 16) & 0xFFu;
    c->r[31] = kReturnAfterGt4;
    psx::cpu::dispatchGuestToReturn0(*c, kGt4, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  }
}

void SopGround::registerOverrides() {
  tomba::native::declareOverlayOverride(
      "SOP", kEntry, "SopGround::draw", &SopGround::draw, psx::present::Producer{psx::present::Arg::A0});
}
