#include "frame/draw_section.h"

#include "core.h"
#include "entry/session.h"
#include "frame/frame_driver.h"
#include "guest_redraw.h"
#include "ordering_table.h"
#include "render/guest_ordering_table.h"

namespace tomba {
namespace {

constexpr std::uint32_t kFieldDraw = 0x8003F9A8u;
constexpr std::uint32_t kRoomDraw = 0x8003FA44u;

} // namespace

std::optional<std::uint32_t> DrawSection::draw(Core &core, psx::present::GuestRedraw &redraw) const {
  // The 2D writers count calls per logic frame; the real draw must count from where the replays began.
  Session &session = sessionOf(&core);
  const render::CallSiteObjects calls = session.callSites;
  const bool returned = redraw.dispatch(variant_ == Variant::Field ? kFieldDraw : kRoomDraw, {});
  session.callSites = calls;
  if (!returned) {
    return std::nullopt;
  }
  return core.mem_r32(render::OrderingTable::kBasePointer) + kOtHeadOffset;
}

bool DrawSection::linked(std::uint32_t head) const {
  // The frame's other chain is PutDrawEnv's packet at env + kDrawEnvOffset.
  for (std::uint32_t buffer = 0; buffer < 2u; ++buffer) {
    if (psx::gpu::mainRamOffsetOf(head) ==
        psx::gpu::mainRamOffsetOf(kDisplayBufferBase + buffer * kDisplayBufferStride + kOtHeadOffset)) {
      return true;
    }
  }
  return false;
}

} // namespace tomba
