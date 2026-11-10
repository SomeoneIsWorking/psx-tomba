#include "frame/draw_section.h"

#include "core.h"
#include "entry/session.h"
#include "frame/frame_driver.h"
#include "guest_redraw.h"
#include "image_identity.h"
#include "ordering_table.h"
#include "render/guest_ordering_table.h"
#include "render/render.h"

#include <string_view>

namespace tomba {
namespace {

// G:8010810C, after either draw in GAME: page 1, the pause menu, links its dim and its items into the same OT.
constexpr std::uint32_t kPageStep = 0x8010810Cu;
constexpr std::string_view kPageStepImage = "GAME";

} // namespace

bool DrawSection::pageStepResident(const Core &core) {
  const psx::cpu::ImageCatalog &images = core.imageCatalog();
  const std::optional<psx::cpu::ImageIdentity> image = images.resolve(kPageStep);
  if (!image) {
    return false;
  }
  const std::optional<psx::cpu::ImageDescription> description = images.describe(*image);
  return description && description->name == kPageStepImage;
}

std::optional<std::uint32_t> DrawSection::draw(Core &core, psx::present::GuestRedraw &redraw) const {
  // The 2D writers count calls per logic frame; the real draw must count from where the replays began.
  Session &session = sessionOf(&core);
  const render::CallSiteObjects calls = session.callSites;
  // The orchestrators' overrides are Render::frame/frameX, which replay; the replay runs their guest bodies.
  const bool returned = redraw.run(selection_.variant == Variant::Field ? Render::kFieldDraw : Render::kRoomDraw, {}) &&
                        (!selection_.pageStep || redraw.dispatch(kPageStep, {}));
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
