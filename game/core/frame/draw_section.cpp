#include "frame/draw_section.h"

#include "core.h"
#include "entry/session.h"
#include "frame/frame_driver.h"
#include "frame/image_residency.h"
#include "guest_redraw.h"
#include "ordering_table.h"
#include "render/guest_ordering_table.h"
#include "render/render.h"
#include "render/tile_grid_layer.h"
#include "scene/demo.h"
#include "scene/narration_scroller.h"
#include "scene/sop.h"

#include <array>
#include <string_view>

namespace tomba {
namespace {

constexpr std::string_view kPageStepImage = "GAME";

} // namespace

bool DrawSection::pageStepResident(const Core &core) {
  return imageHolds(core, kPageStep, kPageStepImage);
}

bool DrawSection::drawSteps(Core &core, psx::present::GuestRedraw &redraw) const {
  switch (selection_.variant) {
  case Variant::Field:
  case Variant::Room:
    // The orchestrators' overrides are Render::frame/frameX, which replay; the replay runs their guest bodies.
    return redraw.run(selection_.variant == Variant::Field ? Render::kFieldDraw : Render::kRoomDraw, {}) &&
           (!selection_.pageStep || redraw.dispatch(kPageStep, {}));
  case Variant::Sop: {
    const std::array scenery{Sop::kSceneryDraw.a0};
    if (!redraw.dispatch(Sop::kSceneryDraw.entry, scenery) || !redraw.dispatch(Sop::kListDraw.entry, {})) {
      return false;
    }
    if (selection_.grid) {
      TileGridLayer::drawSop(core, Sop::kGridDraw.a0);
    }
    if (!selection_.scroller) {
      return true;
    }
    redraw.writeByte(Sop::kInFieldUpdate, 0u);
    return redraw.dispatch(NarrationScroller::kEntry, {});
  }
  case Variant::Page:
    return redraw.run(kPageStep, {});
  case Variant::Narration:
    return redraw.dispatch(NarrationScroller::kEntry, {});
  case Variant::Ride:
    return redraw.run(kRideDraw, {}) && redraw.dispatch(kPageStep, {});
  case Variant::Title: {
    const std::array edges{0u};
    const std::array items{selection_.menu, selection_.item};
    return redraw.dispatch(Demo::kTitleEdges, edges) && redraw.dispatch(Demo::kTitleItems, items);
  }
  }
  return false;
}

std::optional<std::uint32_t> DrawSection::draw(Core &core, psx::present::GuestRedraw &redraw) const {
  // The 2D writers count calls per logic frame; the real draw must count from where the replays began.
  Session &session = sessionOf(&core);
  const render::CallSiteObjects calls = session.callSites;
  const bool returned = drawSteps(core, redraw);
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
