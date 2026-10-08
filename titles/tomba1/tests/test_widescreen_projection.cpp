// Focused hermetic verification of Tomba! 1's guest-widescreen owner.
//
// The claims under test, each with a negative that must fail if the claim stops being true:
//
//  1. the plan this title's own measured geometry produces at 16:9 — projection centre 214, draw
//     width 428, projection 428x224, margin 54, clip right 427, and OFX/H widened by 428/320;
//  2. 4:3 IDENTITY — at PresentationAspect::Standard4x3 the owner applies byte-identical retail
//     values: centre 160, OFY 112, draw width 320, and the same GTE control words (CR24 = 160<<16,
//     CR25 = 112<<16, CR26 = 544) the retail 0x80063A34 body writes;
//  3. H is not this owner's to move — 0x80063A54 stays framework-owned, so CR26 must survive every
//     publication and every frame boundary;
//  4. OFX moves and nothing else does: the 16.16 argument registers, the neighbouring argument
//     registers, and every non-RECT field of the draw environment stay retail;
//  5. the frame boundary does NOT rewrite the GTE when the centre is unchanged, so a 4:3 run writes
//     exactly what 0x80016B04 wrote, once;
//  6. the measured draw width is READ from the publication site, so the 640x480 mode 0x80016DDC
//     builds widens to 854 and not to a hardcoded 428;
//  7. a live aspect change reaches the guest GTE, because 0x80016B04 states the centre once per
//     image and the frame boundary is the only remaining site;
//  8. ASPECT_AUTO stays MatchSink rather than being folded to 16:9 — folding it is exactly how a
//     run reports "wide" while it resolved to 4:3;
//  9. NEGATIVE, each in a forked child because both are abort(): the draw-environment publication
//     refuses before the projection, and an unusable draw width refuses rather than being invented.
#include "context.h"
#include "core.h"
#include "game.h"
#include "game_runtime.h"
#include "guest_widescreen_projection.h"
#include "hw_bind.h"
#include "mods.h"
#include "proj_params.h"
#include "render_mode.h"
#include "widescreen_projection.h"

#include <sys/wait.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>

extern "C" std::uint32_t GTE_ReadCR(unsigned which);
extern "C" void gte_write_ctrl(std::uint32_t reg, std::uint32_t value);

namespace {

// The retail values 0x80016AF8/0x80016AFC/0x80016B0C publish. 0x220 = 544 is the H that 0x8002D89C
// re-asserts per area with the SAME immediate, which is what makes OFX — not H — the only knob a
// horizontal widening may move.
constexpr int kRetailCenterX = 160;
constexpr int kRetailVerticalOffset = 112;
constexpr int kRetailProjectionDistance = 544;
constexpr std::uint32_t kRetailDrawWidth320 = 320;
constexpr std::uint32_t kRetailDrawWidth640 = 640;
constexpr int kUnrelatedRegister = 0x76543210;

int failures = 0;
PresentationAspect g_requested = PresentationAspect::Standard4x3;
// Makes the latch seam return a plan no valid aspect can produce, so the owner's own validation is
// what is under test.
bool g_corruptPlan = false;
GuestProjectionGeometry g_latched{};
int g_latchCount = 0;
int g_retailBodyCalls = 0;
std::uint32_t g_retailBodyLastWidth = 0;

bool check(bool condition, const char *message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
  }
  return condition;
}

// The one latch seam. It stands in for `gpu_vk_latch_guest_projection`, which needs a Core carrying a
// live Game with a real GPU/sink; the plan it returns is built by the framework's OWN pure builder
// from the geometry the OWNER handed over, so the arithmetic under test is the shipping arithmetic
// and nothing is reimplemented here.
GuestProjectionPlan latchPlan(Core *, GuestProjectionGeometry geometry) {
  g_latched = geometry;
  ++g_latchCount;
  GuestProjectionPlan plan = guest_projection_plan({
      .path = RenderPath::Gte,
      .requested = g_requested,
      .nativePresentation = {320, 224},
      .nativeProjection = geometry,
      .sink = {1920, 1080},
      .vramWidth = 1024,
  });
  // The one way the plan can be wrong in a way the pure builder cannot produce: a framework or
  // driver that handed back an unusable plan. The owner must refuse THAT rather than write it, which
  // is a claim about the owner's validation and not about the framework's arithmetic.
  if (g_corruptPlan) {
    plan.projectionCenterX = 0;
    plan.guestDrawWidth = 0;
  }
  return plan;
}

// The guest's SetDefDrawEnv body, standing in for the authenticated original: it writes every
// measured field, so a test can tell a replaced RECT.w from a mutated neighbour.
void retailDrawEnvironment(Core &core) {
  const std::uint32_t environment = core.r[4];
  const auto width = static_cast<std::uint16_t>(core.r[7]);
  core.mem_w16(environment + 0, 0x0180u); // RECT.x, measured 0x180 at 0x80016C78
  core.mem_w16(environment + 2, 0x0100u); // RECT.y, measured 0x100 at 0x80016C7C
  core.mem_w16(environment + 4, width);
  core.mem_w16(environment + 6, 0x00E0u); // RECT.h, measured 0xE0 at 0x80016C88
  core.mem_w32(environment + 8, 0xA1B2C3D4u);
  core.mem_w32(environment + 12, 0x10293847u);
}

void countedRetailDrawEnvironment(Core &core) {
  ++g_retailBodyCalls;
  g_retailBodyLastWidth = core.r[7];
  retailDrawEnvironment(core);
}

void zeroRetailDrawEnvironment(Core &) {}

// The guest's own 0x80063A54, reached through the framework's own H setter, because 0x80063A54 IS
// framework-owned for this title: the title widens OFX and leaves H alone. Reproducing the guest's
// 0x80016AF4 order (SetGeomOffset at 0x80016B04, then SetGeomScreen at 0x80016B0C) is what makes
// `requireGeom` a real observation rather than a tautology.
void publishGuestScreenDistance(Core &core, int h) {
  libgte_set_geom_screen(&core, h);
}

// The framework's recorded projection copy. `requireGeom` aborts when the game never stated one, so
// reaching it IS the claim, and comparing it with the GTE is how the test observes that the register
// and the record did not drift — the failure proj_params.h exists to prevent.
void recordMatchesGte(Core &core, const char *stage) {
  float ofx = 0.0F;
  float ofy = 0.0F;
  float h = 0.0F;
  core.rsub.projParams.requireGeom(stage, ofx, ofy, h);
  check(ofx == static_cast<float>(GTE_ReadCR(24) >> 16), "recorded OFX must equal GTE CR24");
  check(ofy == static_cast<float>(GTE_ReadCR(25) >> 16), "recorded OFY must equal GTE CR25");
  check(h == static_cast<float>(GTE_ReadCR(26)), "recorded H must equal GTE CR26, which this owner never moves");
}

class TestRuntime final : public GameRuntime {
public:
  void *createContext(Core &core) override {
    return new tomba1::Context(core);
  }
  void destroyContext(void *context) override {
    delete static_cast<tomba1::Context *>(context);
  }
  void registerOverrides(Game &) override {}
  void bootInit(Core &) override {}

  RenderCapabilities renderCapabilities() const override {
    return RenderCapabilities::widescreenOnly();
  }
  bool guestVramIsPicture(const Game &) const override {
    return true;
  }
  const GuestWidescreenProjection *guestWidescreenProjection() const override {
    return &policy;
  }

  GuestWidescreenProjection policy;
};

// A child process is the only honest way to observe a refusal: both are std::abort(), so a
// same-process assertion could only ever prove that nothing was checked.
bool childAborts(const char *name, const auto &body) {
  std::fflush(nullptr);
  const pid_t child = fork();
  if (child < 0) {
    std::fprintf(stderr, "FAIL: could not fork the %s negative case\n", name);
    ++failures;
    return false;
  }
  if (child == 0) {
    body();
    // The claim was that this cannot be reached.
    std::fprintf(stderr, "NEGATIVE FAILED: %s returned instead of refusing\n", name);
    std::_Exit(3);
  }
  int status = 0;
  if (waitpid(child, &status, 0) < 0) {
    std::fprintf(stderr, "FAIL: could not wait for the %s negative case\n", name);
    ++failures;
    return false;
  }
  if (!WIFSIGNALED(status) || WTERMSIG(status) != SIGABRT) {
    std::fprintf(stderr, "FAIL: the %s negative case did not refuse (status %d)\n", name, status);
    ++failures;
    return false;
  }
  return true;
}

bool verifyWidePlan(Core &core) {
  g_requested = PresentationAspect::Wide16x9;
  g_latchCount = 0;
  g_retailBodyCalls = 0;
  g_retailBodyLastWidth = 0;
  gte_write_ctrl(26, static_cast<std::uint32_t>(kRetailProjectionDistance));
  tomba1::widescreen::ProjectionOwner owner(latchPlan);
  core.r[4] = static_cast<std::uint32_t>(kRetailCenterX);
  core.r[5] = static_cast<std::uint32_t>(kRetailVerticalOffset);
  owner.publishProjection(core);
  // 0x80016B0C, right after 0x80016B04, through the guest's own framework-owned leaf.
  publishGuestScreenDistance(core, kRetailProjectionDistance);

  if (!check(g_latchCount == 1, "the projection publication must latch exactly once")) {
    return false;
  }
  if (!check(g_latched.extent.width == 320 && g_latched.extent.height == 224 && g_latched.drawWidth == 320,
             "the owner must latch the title's MEASURED 320x224 projection, not a widened literal")) {
    return false;
  }

  const GuestProjectionPlan &plan = owner.plan();
  if (!check(plan.projectionExtent.width == 428 && plan.projectionExtent.height == 224,
             "16:9 must widen the title projection 320x224 -> 428x224")) {
    return false;
  }
  if (!check(plan.projectionCenterX == 214, "16:9 OFX must be the DERIVED projection centre 428/2 = 214")) {
    return false;
  }
  if (!check(plan.projectionHorizontalMargin == 54, "16:9 projection margin must be (428-320)/2 = 54")) {
    return false;
  }
  if (!check(plan.guestDrawWidth == 428 && plan.guestClipRight == 427,
             "16:9 guest draw width must widen 320 -> 428 with clip right 427")) {
    return false;
  }
  if (!check(plan.presentationExtent.width == 428 && plan.presentationHorizontalMargin == 54,
             "16:9 presentation must be 428 wide with margin 54")) {
    return false;
  }
  if (!check(core.r[4] == (214u << 16) && core.r[5] == (112u << 16),
             "the owner must leave both argument registers holding their 16.16 offsets")) {
    return false;
  }
  if (!check(GTE_ReadCR(24) == (214u << 16) && GTE_ReadCR(25) == (112u << 16),
             "the owner must write GTE CR24 = OFX<<16 and CR25 = OFY<<16")) {
    return false;
  }
  if (!check(GTE_ReadCR(26) == static_cast<std::uint32_t>(kRetailProjectionDistance),
             "the owner must not move H: 0x80063A54 stays framework-owned")) {
    return false;
  }
  recordMatchesGte(core, "test-wide-publication");

  // The widening is a pure FOV change: OFX/H must grow by exactly the canvas ratio, and the picture
  // must stay centred. Substituting a smaller H would change central scale instead, which is why
  // this compares the RATIO and not the field of view alone.
  const double retailRatio = static_cast<double>(kRetailCenterX) / kRetailProjectionDistance;
  const double wideRatio = static_cast<double>(plan.projectionCenterX) / kRetailProjectionDistance;
  if (!check(wideRatio / retailRatio > 1.3374 && wideRatio / retailRatio < 1.3376,
             "OFX/H must grow by 428/320 = 1.3375, which IS the widening")) {
    return false;
  }
  if (!check(plan.projectionCenterX * 2 == plan.projectionExtent.width,
             "the widened picture must stay centred, not translated")) {
    return false;
  }

  constexpr std::uint32_t kEnvironment = 0x80110000u;
  core.r[4] = kEnvironment;
  core.r[5] = static_cast<std::uint32_t>(kUnrelatedRegister);
  core.r[6] = static_cast<std::uint32_t>(kUnrelatedRegister);
  core.r[7] = kRetailDrawWidth320;
  owner.publishDrawEnvironment(core, countedRetailDrawEnvironment);
  if (!check(g_retailBodyCalls == 1, "the draw-environment publication must run the retail body once")) {
    return false;
  }
  if (!check(g_retailBodyLastWidth == 428u,
             "the retail body must RECEIVE the widened width as its own argument, not a post-write")) {
    return false;
  }
  if (!check(core.mem_r16(kEnvironment + 0) == 0x0180 && core.mem_r16(kEnvironment + 2) == 0x0100 &&
                 core.mem_r16(kEnvironment + 4) == 428 && core.mem_r16(kEnvironment + 6) == 0x00E0,
             "only RECT.w may change; RECT.x, RECT.y and RECT.h are the measured retail values")) {
    return false;
  }
  if (!check(core.mem_r32(kEnvironment + 8) == 0xA1B2C3D4u && core.mem_r32(kEnvironment + 12) == 0x10293847u,
             "every non-RECT field must remain the retail body's own")) {
    return false;
  }
  if (!check(core.r[5] == static_cast<std::uint32_t>(kUnrelatedRegister) &&
                 core.r[6] == static_cast<std::uint32_t>(kUnrelatedRegister),
             "the publication must not disturb a neighbouring argument register")) {
    return false;
  }
  if (!check(owner.measuredDrawWidth() == kRetailDrawWidth320,
             "the owner must REMEMBER the retail width it measured, not the widened one")) {
    return false;
  }

  // The widening must not COMPOUND. A frame boundary re-latches from the measured retail width, so
  // the width stays 428; an owner that remembered its own widened output would re-widen 428 to 572
  // on the very next frame, and keep going.
  owner.synchronizePresentation(core);
  if (!check(owner.plan().guestDrawWidth == 428 && owner.plan().nativeGuestDrawWidth == 320,
             "a frame boundary must re-latch from the MEASURED width, so the widening cannot compound")) {
    return false;
  }
  owner.synchronizePresentation(core);
  if (!check(owner.plan().guestDrawWidth == 428, "a second frame boundary must not widen the picture again")) {
    return false;
  }

  // The 640x480 display mode 0x80016DDC builds (RECT.w = 0x280 at 0x80016E10/0x80016E54).
  core.r[4] = kEnvironment;
  core.r[7] = kRetailDrawWidth640;
  owner.publishDrawEnvironment(core, retailDrawEnvironment);
  const GuestProjectionPlan wide640 = owner.plan();
  if (!check(core.mem_r16(kEnvironment + 4) == 854,
             "a 640-wide display mode must widen to 854, the smallest even width reaching 4:3")) {
    return false;
  }
  if (!check(wide640.guestDrawWidth == 854 && wide640.nativeGuestDrawWidth == 640,
             "the 640 mode's widened width must be DERIVED from the width the guest published")) {
    return false;
  }
  if (!check(wide640.projectionCenterX == 214 && wide640.projectionExtent.width == 428,
             "the GTE projection extent is a title constant: SetGeomOffset has one caller in the image")) {
    return false;
  }
  return true;
}

bool verifyStandardIdentity(Core &core) {
  g_requested = PresentationAspect::Standard4x3;
  gte_write_ctrl(24, 0u);
  gte_write_ctrl(25, 0u);
  gte_write_ctrl(26, static_cast<std::uint32_t>(kRetailProjectionDistance));
  tomba1::widescreen::ProjectionOwner owner(latchPlan);
  core.r[4] = static_cast<std::uint32_t>(kRetailCenterX);
  core.r[5] = static_cast<std::uint32_t>(kRetailVerticalOffset);
  owner.publishProjection(core);
  publishGuestScreenDistance(core, kRetailProjectionDistance);

  if (!check(core.r[4] == (160u << 16) && core.r[5] == (112u << 16),
             "4:3 must leave the 16.16 offsets byte-identical to what 0x80063A34 wrote")) {
    return false;
  }
  if (!check(GTE_ReadCR(24) == (160u << 16) && GTE_ReadCR(25) == (112u << 16) &&
                 GTE_ReadCR(26) == static_cast<std::uint32_t>(kRetailProjectionDistance),
             "4:3 must write the retail CR24/CR25/CR26 the guest's own leaf would have written")) {
    return false;
  }
  recordMatchesGte(core, "test-4x3-publication");

  const GuestProjectionPlan &plan = owner.plan();
  if (!check(plan.projectionCenterX == 160 && plan.guestDrawWidth == 320 && plan.projectionClipRight == 319 &&
                 plan.guestClipRight == 319,
             "4:3 must resolve to the exact retail centre and clip extents")) {
    return false;
  }
  if (!check(!plan.widescreen() && plan.presentationExtent.width == 320 && plan.presentationHorizontalMargin == 0 &&
                 plan.projectionHorizontalMargin == 0,
             "4:3 must not widen and must report no margin, so the host presents 320")) {
    return false;
  }

  constexpr std::uint32_t kEnvironment = 0x80120000u;
  core.r[4] = kEnvironment;
  core.r[7] = kRetailDrawWidth320;
  owner.publishDrawEnvironment(core, retailDrawEnvironment);
  if (!check(core.mem_r16(kEnvironment + 4) == 320,
             "4:3 must publish the retail 320 RECT.w into the draw environment")) {
    return false;
  }

  // A frame boundary at an unchanged aspect must not touch the GTE at all, so a 4:3 run writes
  // exactly what 0x80016B04 wrote, once, and never again.
  gte_write_ctrl(24, 0xDEADBEEFu);
  owner.synchronizePresentation(core);
  if (!check(GTE_ReadCR(24) == 0xDEADBEEFu,
             "an unchanged centre must not be re-asserted: 4:3 must leave the GTE alone per frame")) {
    return false;
  }
  return true;
}

bool verifyLiveAspectChange(Core &core) {
  g_requested = PresentationAspect::Standard4x3;
  gte_write_ctrl(24, 0u);
  gte_write_ctrl(25, 0u);
  gte_write_ctrl(26, static_cast<std::uint32_t>(kRetailProjectionDistance));
  tomba1::widescreen::ProjectionOwner owner(latchPlan);
  core.r[4] = static_cast<std::uint32_t>(kRetailCenterX);
  core.r[5] = static_cast<std::uint32_t>(kRetailVerticalOffset);
  owner.publishProjection(core);
  publishGuestScreenDistance(core, kRetailProjectionDistance);
  core.r[4] = 0x80130000u;
  core.r[7] = kRetailDrawWidth320;
  owner.publishDrawEnvironment(core, retailDrawEnvironment);
  if (!check(GTE_ReadCR(24) == (160u << 16), "the run must start at the retail centre")) {
    return false;
  }

  // The player turns widescreen on while the game runs. 0x80016B04 states the centre once per image,
  // so the frame boundary is the only site that can carry the change to the GTE.
  g_requested = PresentationAspect::Wide16x9;
  owner.synchronizePresentation(core);
  if (!check(GTE_ReadCR(24) == (214u << 16), "a live aspect change must reach the guest GTE")) {
    return false;
  }
  if (!check(GTE_ReadCR(25) == (112u << 16), "re-asserting the live centre must not move OFY")) {
    return false;
  }
  if (!check(GTE_ReadCR(26) == static_cast<std::uint32_t>(kRetailProjectionDistance),
             "re-asserting the live centre must still not move H")) {
    return false;
  }
  if (!check(owner.plan().presentationExtent.width == 428, "the re-latched plan must be the wide one")) {
    return false;
  }
  return true;
}

} // namespace

int main() {
  // The GTE control-register file is Beetle's module-global storage, so it must exist before any
  // CR read or write. main.cpp calls the same initialiser in the product.
  gte_init();
  TestRuntime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;

  if (!verifyWidePlan(core) || !verifyStandardIdentity(core) || !verifyLiveAspectChange(core)) {
    std::fputs("widescreen projection claims failed\n", stderr);
    return 1;
  }

  // NEGATIVE 1: a draw clip widened against a projection the title never stated. X4 refuses the
  // same unpaired publication, and for the same reason.
  if (!childAborts("unpaired draw environment", [&core]() {
        tomba1::widescreen::ProjectionOwner owner(latchPlan);
        core.r[4] = 0x80140000u;
        core.r[7] = kRetailDrawWidth320;
        owner.publishDrawEnvironment(core, zeroRetailDrawEnvironment);
      })) {
    return 1;
  }

  // NEGATIVE 2: an unusable published draw width. A plausible default here would be a guess about
  // a display mode nobody measured, which is the thing proj_params.h exists to refuse.
  if (!childAborts("zero draw width", [&core]() {
        tomba1::widescreen::ProjectionOwner owner(latchPlan);
        core.r[4] = static_cast<std::uint32_t>(kRetailCenterX);
        core.r[5] = static_cast<std::uint32_t>(kRetailVerticalOffset);
        owner.publishProjection(core);
        tomba1::widescreen::ProjectionOwner::measuredGeometry(0u);
      })) {
    return 1;
  }

  // NEGATIVE 3: the latch returns a plan no aspect can produce. The owner must refuse it rather than
  // write a zero centre and a zero draw width into the GTE, which would black the picture out and
  // still report a widened host canvas.
  if (!childAborts("unusable latched plan", [&core]() {
        g_corruptPlan = true;
        g_requested = PresentationAspect::Wide16x9;
        tomba1::widescreen::ProjectionOwner owner(latchPlan);
        core.r[4] = static_cast<std::uint32_t>(kRetailCenterX);
        core.r[5] = static_cast<std::uint32_t>(kRetailVerticalOffset);
        owner.publishProjection(core);
      })) {
    return 1;
  }
  g_corruptPlan = false;

  if (std::fflush(nullptr) != 0 || failures != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
  }
  std::puts("PASS: 16:9 plan, 4:3 identity, untouched H, non-compounding re-latch, live aspect, "
            "and three refusals hold");
  return 0;
}
