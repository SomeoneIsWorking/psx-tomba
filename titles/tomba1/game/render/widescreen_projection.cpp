#include "widescreen_projection.h"

#include "context.h"
#include "core.h"
#include "execution_control.h"
#include "game.h"
#include "gpu_vk.h"
#include "mods.h"
#include "native_dispatch.h"
#include "proj_params.h"

#include <cstdlib>
#include <lucent/log.h>

namespace tomba1::widescreen {
namespace {

// libgte SetGeomOffset's argument registers, and the values the framework's ProjParams records as
// unset. proj_params.h states the record's defaults are 0 and NOT the stock 160/120/350, so "the
// game never stated a projection" can never be mistaken for "the game stated the stock projection";
// this owner therefore checks the record rather than the GTE, which the guest has since moved.
constexpr int kArgumentA0 = 4;
constexpr int kArgumentA1 = 5;
constexpr int kArgumentA3 = 7;

// SCUS_942.36's libgpu SetDefDrawEnv, 0x8005DC9C. The address is declared in this owner's header,
// beside the census that justifies treating it as an override entry.

constexpr int kArgumentA0Shifted16 = 16;
constexpr int kWidestGuestClip = 0xFFFF;

[[noreturn]] void refuse(const char *what) {
  lucent::error("tomba1-wide", "SCUS_942.36 guest widescreen {}", what);
  std::abort();
}

} // namespace

ProjectionOwner::ProjectionOwner() : ProjectionOwner(gpu_vk_latch_guest_projection) {}

ProjectionOwner::ProjectionOwner(Latch latch) : latch_(latch) {
  if (!latch_) {
    refuse("requires the shared plan latch");
  }
}

GuestProjectionGeometry ProjectionOwner::measuredGeometry(std::uint32_t drawWidth) {
  if (drawWidth == 0 || drawWidth > static_cast<std::uint32_t>(kWidestGuestClip)) {
    lucent::error("tomba1-wide", "SCUS_942.36 published an unusable draw width {}", drawWidth);
    std::abort();
  }
  // The projection extent is the title's own, because SetGeomOffset has exactly one caller in the
  // image and therefore publishes 320x224 in every display mode. Only the draw width is per-mode.
  return {kMeasured320x224.extent, static_cast<int>(drawWidth)};
}

GuestProjectionPlan ProjectionOwner::relatch(Core &core, GuestProjectionGeometry geometry) {
  GuestProjectionPlan latched = latch_(&core, geometry);
  if (latched.projectionCenterX <= 0 || latched.guestDrawWidth <= 0 || latched.guestDrawWidth > kWidestGuestClip) {
    lucent::error("tomba1-wide",
                  "the framework returned an unusable guest projection (centre={}, draw width={})",
                  latched.projectionCenterX,
                  latched.guestDrawWidth);
    std::abort();
  }
  plan_ = latched;
  return latched;
}

void ProjectionOwner::publishProjection(Core &core) {
  if (!core.game) {
    refuse("reached a Core with no Game");
  }
  // The retail centre is read from $a0 rather than assumed, so a future second caller publishing a
  // different centre widens THAT centre instead of being silently replaced by a literal.
  const int retailCenterX = static_cast<std::int32_t>(core.r[kArgumentA0]);
  verticalOffset_ = static_cast<std::int32_t>(core.r[kArgumentA1]);
  const GuestProjectionPlan latched = relatch(core, kMeasured320x224);
  const auto centerX = latched.projectionCenterX;
  appliedCenterX_ = centerX;

  // 0x80063A34/0x80063A38 are `sll $a0, $a0, 16` / `sll $a1, $a1, 16`: the leaf leaves both argument
  // registers holding the 16.16 offsets. `libgte_set_geom_offset` is the framework's own single
  // implementation of CR24/CR25 and of the recorded projection copy, so the GTE and the record
  // cannot drift. OFY and H are untouched: H is 0x80063A54's business, and the vertical FOV is not
  // what a horizontal widening may change.
  core.r[kArgumentA0] = static_cast<std::uint32_t>(centerX) << kArgumentA0Shifted16;
  core.r[kArgumentA1] = static_cast<std::uint32_t>(verticalOffset_) << kArgumentA0Shifted16;
  libgte_set_geom_offset(&core, centerX, verticalOffset_);

  projectionPublished_ = true;
  lucent::info("tomba1-wide",
               "guest projection {}x{} -> {}x{}, OFX {} -> {}, OFY {}, draw width {} -> {}",
               latched.nativeProjectionExtent.width,
               latched.nativeProjectionExtent.height,
               latched.projectionExtent.width,
               latched.projectionExtent.height,
               retailCenterX,
               centerX,
               verticalOffset_,
               latched.nativeGuestDrawWidth,
               latched.guestDrawWidth);
}

void ProjectionOwner::publishDrawEnvironment(Core &core, const RetailLeaf &retail) {
  if (!retail) {
    refuse("draw-environment publication requires the retail SetDefDrawEnv body");
  }
  if (!projectionPublished_) {
    // X4 refuses the same unpaired publication, and for the same reason: widening a draw clip
    // against a projection the title never stated is a clip with nothing to centre it in.
    refuse("draw-environment publication ran before 0x80063A34 stated a projection");
  }

  const auto retailDrawWidth = static_cast<std::uint32_t>(core.r[kArgumentA3]);
  const GuestProjectionPlan latched = relatch(core, measuredGeometry(retailDrawWidth));
  drawWidth_ = retailDrawWidth;
  // RECT.w is the only measured field replaced. The argument, not the environment, is replaced, so
  // the retail body never observes a mutation and remains the authority for x, y, h and the rest.
  core.r[kArgumentA3] = static_cast<std::uint32_t>(latched.guestDrawWidth);
  retail(core);
}

void ProjectionOwner::synchronizePresentation(Core &core) {
  if (!projectionPublished_) {
    return;
  }
  if (drawWidth_ == 0) {
    // Site 1 ran but site 2 has not: the guest has not stated a draw width yet, so there is no
    // per-mode geometry to re-derive a plan from. The boot plan is already latched and correct.
    return;
  }
  const GuestProjectionPlan latched = relatch(core, measuredGeometry(drawWidth_));
  if (latched.projectionCenterX == appliedCenterX_) {
    return;
  }
  appliedCenterX_ = latched.projectionCenterX;
  libgte_set_geom_offset(&core, appliedCenterX_, verticalOffset_);
  lucent::info("tomba1-wide",
               "aspect changed; re-asserted guest OFX {} for draw width {}",
               appliedCenterX_,
               latched.guestDrawWidth);
}

// The native entry at 0x8005DC9C. Not reachable through PlatformHle, which is why the title
// installs it itself; the retail body is always the authenticated original guest function executed
// through Lightrec, so `publishDrawEnvironment` cannot drift from what the guest really writes.
void drawEnvironmentOverride(Core *core) {
  if (!core) {
    refuse("SetDefDrawEnv reached without a Core");
  }
  context(*core).widescreen.publishDrawEnvironment(*core, [](Core &target) {
    psx::cpu::callOriginalToReturn(
        target, kSetDefDrawEnv, psx::cpu::ExecutionBudget::currentTurn(target), "tomba1-wide::SetDefDrawEnv original");
  });
}

} // namespace tomba1::widescreen
