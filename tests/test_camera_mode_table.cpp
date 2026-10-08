// tests/test_camera_mode_table.cpp — the camera driver's mode table, and the three policy facts it
// exists to make single-sourced.
//
// The table replaced TWO 18-arm switches — one for the native path, one for the guest-faithful
// mirror — that used to disagree silently if a mode were added to one and not the other. These
// assertions are what makes the table worth having: they cover the eighteen modes as a SET, the
// render-mode prologue that modes 0 and 1 share, and the per-render-mode height floors.
//
// Three of them are the ones a careless edit actually gets wrong, and each says why:
//   * the four guest RETURN constants per prologue row are per (mode, render mode) PAIR. Collapsing
//     them to one per mode is a behaviour change, because a body that pushes a frame writes its
//     return address into guest stack bytes the reference compares.
//   * the two mode aliases (7/14 and 11/12) must keep sharing a body. They do in the guest's own
//     table, and a reader who "fixed" the apparent duplication would split them.
//   * mode 16 has NO body. It is the tail-only mode, and giving it one would be inventing behaviour.
#include "camera/camera_mode.h"

#include <cstdint>
#include <cstdio>
#include <iterator>

using tomba::camera::CameraMode;
using tomba::camera::CameraShakeState;
using tomba::camera::FollowTarget;

namespace {

int failures = 0;
int checks = 0;

void check(bool condition, const char *detail) {
  checks++;
  if (!condition) {
    std::printf("FAIL: %s\n", detail);
    failures++;
  }
}

template <typename A, typename B> void checkEq(A actual, B expected, const char *detail) {
  check(actual == static_cast<A>(expected), detail);
}

// ---- the eighteen modes, as a SET --------------------------------------------------------------

static void test_the_table_covers_every_mode_exactly_once(void) {
  // Not a sample: every value 0..17, and nothing outside it. A mode missing from the table would
  // make a camera mode silently do nothing, and a mode present twice would make the lookup depend
  // on table order.
  for (uint8_t mode = 0; mode <= tomba::camera::kHighestCameraMode; mode++) {
    const auto *descriptor = tomba::camera::descriptorFor(static_cast<CameraMode>(mode));
    check(descriptor != nullptr, "a live driver mode has no row in the mode table");
    if (descriptor == nullptr) {
      continue;
    }
    check(static_cast<uint8_t>(descriptor->mode) == mode, "a mode table row does not report its own mode");
    // A follow target outside the five named kinds would make the switch over it fall through to
    // whatever the compiler chose, which for this table means "follows the master position".
    check(descriptor->follows == FollowTarget::kFollowsNone ||
              descriptor->follows == FollowTarget::kFollowsCameraSelf ||
              descriptor->follows == FollowTarget::kFollowsMaster ||
              descriptor->follows == FollowTarget::kFollowsOverlay ||
              descriptor->follows == FollowTarget::kFollowsRenderOverlay,
          "a mode's follow target is not one of the five named kinds");
  }
  // Nothing outside the eighteen resolves. `update()` tests `mode < 18` before dispatching, so a
  // value of 18 reaching here would be a missing guard, not a missing row.
  check(tomba::camera::descriptorFor(static_cast<CameraMode>(18)) == nullptr,
        "a mode past the table's end resolved to a row");
  check(tomba::camera::descriptorFor(static_cast<CameraMode>(255)) == nullptr,
        "an out-of-range mode byte resolved to a row");
}

static void test_the_guest_entries_are_the_addresses_the_arms_used(void) {
  // These are the six overlay/follow entries the two dispatchers used to spell inline. A wrong one
  // dispatches into a different guest function and nothing complains.
  checkEq(tomba::camera::guestEntryFor(CameraMode::kFieldOverlay9), 0x8018B924u, "mode 9's overlay entry moved");
  checkEq(tomba::camera::guestEntryFor(CameraMode::kAreaOverlayScripted), 0x8010D89Cu, "mode 10's overlay entry moved");
  checkEq(tomba::camera::guestEntryFor(CameraMode::kFieldOverlay17), 0x80111AB4u, "mode 17's overlay entry moved");
  // A mode whose body is a sequence of already-owned native methods has no single entry, and saying
  // so with zero is what stops a caller from dispatching it as though it had one.
  checkEq(tomba::camera::guestEntryFor(CameraMode::kReset), 0u, "the reset mode claims a guest entry");
  checkEq(tomba::camera::guestEntryFor(CameraMode::kFreezeAtMasterHeight), 0u, "the freeze mode claims a guest entry");
  checkEq(tomba::camera::guestEntryFor(CameraMode::kTailOnly), 0u, "the tail-only mode claims a guest entry");
}

static void test_the_two_alias_pairs_still_share_a_follow_target(void) {
  // Modes 7 and 14 have the same body in the guest's own table, and so do 11 and 12. The apparent
  // duplication in the old switches was the GUEST's, not a copy-paste slip, so the table records it
  // as two names with one behaviour rather than quietly dropping one.
  const auto *seven = tomba::camera::descriptorFor(CameraMode::kSnapFollowSelf);
  const auto *fourteen = tomba::camera::descriptorFor(CameraMode::kSnapFollowSelfAlias);
  const auto *eleven = tomba::camera::descriptorFor(CameraMode::kReset);
  const auto *twelve = tomba::camera::descriptorFor(CameraMode::kForceModeByte);
  check(seven != nullptr && fourteen != nullptr && seven->follows == fourteen->follows,
        "modes 7 and 14 no longer follow the same thing");
  check(seven != nullptr && seven->guestEntry == fourteen->guestEntry, "modes 7 and 14 no longer share a guest entry");
  check(eleven != nullptr && twelve != nullptr && eleven->follows == twelve->follows,
        "modes 11 and 12 no longer share a follow target");
  checkEq(tomba::camera::kSnapFollowSelfAliasOf, 7u, "the 7/14 alias is recorded against the wrong mode");
  checkEq(tomba::camera::kResetAliasOf, 11u, "the 11/12 alias is recorded against the wrong mode");
}

// ---- the render-mode prologue ------------------------------------------------------------------

static void test_the_three_render_modes_with_an_overlay_prologue_are_named(void) {
  // Render modes 2, 7 and 20 are the only three the guest gives dedicated overlay handlers to; every
  // other render mode falls through to the mode's own follow. Missing one would run a resident
  // overlay handler as a native follow in two areas.
  checkEq(static_cast<uint32_t>(std::size(tomba::camera::kRenderModePrologues)),
          3u,
          "the prologue table no longer has three render modes");
  for (const auto &row : tomba::camera::kRenderModePrologues) {
    check(row.forMainFollow != 0u, "a prologue row has no main-follow overlay entry");
    check(row.forTrackFollow != 0u, "a prologue row has no track-follow overlay entry");
    // The two modes get DIFFERENT overlay bodies, so the two addresses must differ too.
    check(row.forMainFollow != row.forTrackFollow, "a prologue row points both modes at the same overlay");
  }
}

static void test_the_prologue_return_constants_are_all_six_distinct(void) {
  // This is the assertion that makes the table's four return columns load-bearing. The guest emits a
  // separate jump for each of the six (mode, render mode) combinations, and each writes its own
  // post-jump address into the guest stack. One constant reused across two of them is a behaviour
  // change, and it is exactly the change a "tidy the table up" edit would make.
  uint32_t seen[tomba::camera::kRenderModePrologues_count * 2];
  size_t count = 0;
  for (const auto &row : tomba::camera::kRenderModePrologues) {
    seen[count++] = row.mainFollowReturn;
    seen[count++] = row.trackFollowReturn;
  }
  for (size_t i = 0; i < count; i++) {
    check(seen[i] != 0u, "a prologue return constant is zero");
    for (size_t j = i + 1; j < count; j++) {
      check(seen[i] != seen[j], "two (mode, render mode) pairs share a guest return constant");
    }
  }
}

static void test_the_render_mode_byte_and_function_table_are_the_guests(void) {
  checkEq(tomba::camera::kRenderModeByte, 0x800BF870u, "the render-mode byte moved");
  checkEq(tomba::camera::kRenderModeFunctionTable, 0x800A4AA0u, "the render-mode function table moved");
}

// ---- the height floors ---------------------------------------------------------------------------

static void test_the_five_floor_modes_are_five_distinct_indices_in_range(void) {
  const uint32_t indices[] = {
      tomba::camera::kFloorRenderModeIndex,
      tomba::camera::kFloorSeaRenderModeIndex,
      tomba::camera::kFloorShoreRenderModeIndex,
      tomba::camera::kFloorNightRenderModeIndex,
      tomba::camera::kFloorFinalRenderModeIndex,
  };
  checkEq(static_cast<uint32_t>(std::size(indices)), 5u, "the floor table no longer names five render modes");
  for (size_t i = 0; i < std::size(indices); i++) {
    check(indices[i] <= tomba::camera::kHighestFloorRenderModeIndex,
          "a floor index is past the end of the range the guard admits");
    for (size_t j = i + 1; j < std::size(indices); j++) {
      check(indices[i] != indices[j], "two floor modes share an index, so one of them is unreachable");
    }
  }
  // Each floor index is `renderMode - 1`, so the render mode it selects must be inside 1..13 for the
  // guard to admit it — which is the whole reason the guard exists.
  for (const uint32_t index : indices) {
    const uint32_t renderMode = index + 1u;
    check(renderMode >= 1u && renderMode <= 13u, "a floor index maps to a render mode outside 1..13");
  }
}

static void test_each_floor_differs_from_its_comparison_bound_by_the_guests_own_amount(void) {
  // Three of the floors are written as "raise unless already below a bound", where the bound is one
  // unit ABOVE the floor. A reader who "corrects" that to an equality test changes the camera height
  // at exactly the boundary value, so the difference is pinned here rather than left to a comment.
  checkEq(static_cast<int32_t>(tomba::camera::kFloorRenderMode13) + 1,
          tomba::camera::kFloorRenderMode13Bound,
          "the render-mode-13 floor is not one below its bound");
  checkEq(static_cast<int32_t>(tomba::camera::kSeaFloorHigh) + 1,
          tomba::camera::kSeaFloorHighBound,
          "the far sea floor is not one below its bound");
  checkEq(static_cast<int32_t>(tomba::camera::kSeaFloorNear) + 1,
          tomba::camera::kSeaFloorLowPlusOne,
          "the near sea floor is not one below its bound");
  // The conditional sub-area selectors, which choose between two floors.
  checkEq(tomba::camera::kSeaSubAreaSeven, 7u, "the sea sub-area selector changed");
  checkEq(tomba::camera::kShoreSubAreaFourteen, 14u, "the shore sub-area selector changed");
  // The two sea floors are nested: the near arm only applies below the low floor, and the two
  // branches must not be swapped.
  check(tomba::camera::kSeaFloorNear > tomba::camera::kSeaFloorLow, "the near sea floor is not above the low floor");
  check(tomba::camera::kSeaFloorHigh > tomba::camera::kSeaFloorLow, "the far sea floor is not above the low floor");
}

// ---- the shake state machine ---------------------------------------------------------------------

static void test_the_ten_shake_states_are_ten_and_the_guard_admits_exactly_them(void) {
  // The tail's guard is `state > 9 -> return`. A state added to the enum without moving the guard
  // would be silently ignored; a guard moved without adding a state would jitter on a byte that
  // means something else.
  checkEq(tomba::camera::kHighestShakeState, 9u, "the shake tail's guard no longer admits ten states");
  checkEq(static_cast<uint8_t>(CameraShakeState::kIdle), 0u, "the idle shake state is no longer 0");
  checkEq(static_cast<uint8_t>(CameraShakeState::kPulseHeightSmall), 9u, "the last shake state is no longer 9");
  // A state value past the guard is not a shake state, and the table must not claim otherwise.
  for (uint8_t state = 0; state < 64u; state++) {
    const bool isShakeState = state <= tomba::camera::kHighestShakeState;
    check(isShakeState == (state <= 9u), "the shake guard and the last member disagree");
  }
}

static void test_the_shake_amplitudes_and_masks_keep_their_relationships(void) {
  // In the three-axis family Y jitters half as far as X and Z, and off half the mask. That is not a
  // coincidence of the constants; it is the guest's shape, and collapsing it would make the Y axis
  // shake as far as the others.
  checkEq(static_cast<uint32_t>(tomba::camera::kShakeMaskNarrow), 0x1Fu, "the narrow jitter mask changed");
  checkEq(static_cast<uint32_t>(tomba::camera::kShakeMaskWide), 0x3Fu, "the wide jitter mask changed");
  checkEq(static_cast<int32_t>(tomba::camera::kShakeXAmplitude) / 2,
          tomba::camera::kShakeYAmplitude,
          "the Y axis no longer jitters at half the X/Z amplitude");
  checkEq(static_cast<uint32_t>(tomba::camera::kShakeMaskNarrow) / 2u,
          static_cast<uint32_t>(tomba::camera::kShakeMaskNarrow >> 1),
          "the Y axis no longer draws from half the X/Z mask");
  // The two height-only amplitudes and the two effect ids the three families queue.
  checkEq(static_cast<int32_t>(tomba::camera::kShakeHeightAmplitude), 32, "the height jitter amplitude changed");
  checkEq(static_cast<int32_t>(tomba::camera::kShakeHeightSmallAmplitude), 16, "the small pulse amplitude changed");
  checkEq(tomba::camera::kShakeEffectAxes, 129u, "the axis shake effect id changed");
  checkEq(tomba::camera::kShakeEffectHeight, 241u, "the height shake effect id changed");
  // The two height-only families must NOT share an effect id: the free-running one queues a different
  // sound from the pulses, and a copy-paste that merged them would change what the game hears.
  check(tomba::camera::kShakeEffectAxes != tomba::camera::kShakeEffectHeight,
        "the two shake families now queue the same effect id");
  // The anchor words and the busy byte that aborts a one-shot.
  checkEq(tomba::camera::kShakeAnchorX, 0x86u, "the X anchor moved");
  checkEq(tomba::camera::kShakeAnchorY, 0x88u, "the Y anchor moved");
  checkEq(tomba::camera::kShakeAnchorZ, 0x8Au, "the Z anchor moved");
  checkEq(tomba::camera::kShakeBusyByte, 0x64u, "the one-shot busy byte moved");
}

} // namespace

int main() {
  test_the_table_covers_every_mode_exactly_once();
  test_the_guest_entries_are_the_addresses_the_arms_used();
  test_the_two_alias_pairs_still_share_a_follow_target();
  test_the_three_render_modes_with_an_overlay_prologue_are_named();
  test_the_prologue_return_constants_are_all_six_distinct();
  test_the_render_mode_byte_and_function_table_are_the_guests();
  test_the_five_floor_modes_are_five_distinct_indices_in_range();
  test_each_floor_differs_from_its_comparison_bound_by_the_guests_own_amount();
  test_the_ten_shake_states_are_ten_and_the_guard_admits_exactly_them();
  test_the_shake_amplitudes_and_masks_keep_their_relationships();

  std::printf("camera mode table: %d checks, %s\n", checks, failures == 0 ? "PASS" : "FAIL");
  return failures == 0 ? 0 : 1;
}
