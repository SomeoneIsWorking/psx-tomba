// Tomba! 1 (SCUS_942.36) guest widescreen: the title's own projection, draw-clip and presentation
// owner. Every number below was read out of the authenticated USA executable; the address that
// carries each fact is in the comment that states it. Nothing here is a tuned constant.
#pragma once

#include "guest_widescreen_projection.h"

#include <cstdint>

class Core;

namespace tomba1::widescreen {

// SCUS_942.36's guest libgpu SetDefDrawEnv. The census behind the claim that an override entry here
// is exact: 139264 instruction words of .text decoded, 4 direct `jal` callers (0x80016C94,
// 0x80016CC8, 0x80016E24, 0x80016E58), 0 references through a register or a pointer word — so `jal`
// reaches this entry and no caller reaches a label inside its body.
inline constexpr std::uint32_t kSetDefDrawEnv = 0x8005DC9Cu;

// The native entry for that address. It is NOT a PlatformHle binding: the framework's builtin table
// covers the two libgte projection leaves and the stock library services, and a guest libgpu
// rectangle constructor is none of those, so the title installs it in its own override registry.
void drawEnvironmentOverride(Core *core);

// The two measured publication sites and the pure geometry decision both are derived from.
//
// A PSX's horizontal field of view is the ratio OFX/H, not H (psxport/docs/presentation-contract.md,
// "What counts as a widening, and what actually breaks one"). SCUS_942.36 publishes OFX/OFY once at
// 0x80016AF4 and re-asserts H per area at 0x8002D784, both through the two libgte leaves below, so
// the only widening that leaves central scale and the vertical FOV untouched is to move OFX outward
// at unchanged H. This owner replaces exactly one argument at each site and nothing else.
class ProjectionOwner {
public:
  // The framework's own latch, injected so a hermetic test drives the production path.
  using Latch = GuestProjectionPlan (*)(Core *, GuestProjectionGeometry);
  // An authenticated original guest body, executed through Lightrec. Injected so a test can observe
  // the transformation without a guest image.
  using RetailLeaf = void (*)(Core &);

  ProjectionOwner();
  explicit ProjectionOwner(Latch latch);

  // --- site 1: libgte SetGeomOffset, 0x80063A34 --------------------------------------------------
  // The image's SOLE caller is 0x80016B04 inside 0x80016AF4, which materialises `addiu $a0, $zero,
  // 0xA0` (160) and `addiu $a1, $zero, 0x70` (112) at 0x80016AF8/0x80016AFC. A census of all 139264
  // instruction words of .text found 1 direct `jal` and 0 register or pointer references, so the
  // picture's horizontal centre is stated exactly once, here. Latches the plan, replaces $a0 with
  // the plan's projection centre, and performs the leaf's own measured behaviour.
  void publishProjection(Core &core);

  // --- site 2: libgpu SetDefDrawEnv, 0x8005DC9C --------------------------------------------------
  // RECT.w arrives in $a3 from 0x80016C94/0x80016CC8 (the 320x224 rectangles) and from
  // 0x80016E24/0x80016E58 (the 640x480 rectangles built by 0x80016DDC). The display mode is a
  // runtime choice reached from 0x80019488/0x80019B14, so the draw width is READ here and never
  // hardcoded. Replaces $a3 with the plan's guest draw width, then runs the authenticated original
  // body, leaving it the authority for RECT.x, RECT.y, RECT.h and every non-RECT field.
  void publishDrawEnvironment(Core &core, const RetailLeaf &retail);

  // The frame boundary. Re-latches the current policy and re-asserts the guest projection centre
  // ONLY when that centre actually differs from the one last written, so a 4:3 run writes exactly
  // what retail wrote at 0x80016B04 and never again, while a live aspect change reaches the GTE
  // instead of widening only the host canvas around an un-widened guest frustum.
  void synchronizePresentation(Core &core);

  // The title's measured GTE projection extent. `FUN_80016AF4` publishes SetGeomOffset(160, 112)
  // (0x80016AF8/0x80016AFC) and SetGeomScreen(544) (0x80016B0C, re-asserted 0x8002D89C with the
  // same 0x220), which is a 320x224 projection centred at (160, 112); `FUN_80016C4C` builds 320x224
  // draw rectangles (RECT.w = 0x140 at 0x80016C80). 320 is the draw width, never the frustum: the
  // plan widens the projection to 428 and DERIVES OFX = 428/2 = 214 from it.
  static constexpr GuestProjectionGeometry kMeasured320x224{{320, 224}, 320};

  // The same title projection paired with a display mode's own live RECT.w. The projection extent is
  // a title constant because SetGeomOffset has one caller in the whole image; the draw width varies
  // per display mode, so it is an argument. An unusable width refuses rather than inventing a
  // plausible default.
  static GuestProjectionGeometry measuredGeometry(std::uint32_t drawWidth);

  const GuestProjectionPlan &plan() const {
    return plan_;
  }

  bool projectionPublished() const {
    return projectionPublished_;
  }

  // The last draw width the title measured at site 2, or 0 before site 2 has run.
  std::uint32_t measuredDrawWidth() const {
    return drawWidth_;
  }

private:
  // Re-latch, validate the returned plan, and keep it. Refuses a plan the framework or a wrong
  // aspect would have made unusable rather than applying a plausible-looking one.
  GuestProjectionPlan relatch(Core &core, GuestProjectionGeometry geometry);

  Latch latch_;
  GuestProjectionPlan plan_;
  std::uint32_t drawWidth_ = 0;
  int verticalOffset_ = 0;
  int appliedCenterX_ = 0;
  bool projectionPublished_ = false;
};

} // namespace tomba1::widescreen
