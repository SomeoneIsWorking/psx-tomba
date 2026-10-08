// game/camera/camera_mode.h — the camera DRIVER'S MODE TABLE, in one place.
//
// WHY THIS FILE EXISTS. `CutsceneCamera` ran the same eighteen modes through two independent 18-arm
// switches: `dispatchMode` for the native path and `dispatchModeFaithful` for the guest-faithful
// mirror. They agreed on which mode did what and disagreed only on HOW they reached it — one called
// a C++ method, the other dispatched to the guest address with that arm's own return constant. Two
// tables describing one policy is duplicated policy, and it is the kind that drifts: a thirteenth
// mode added to one switch and not the other would run a different camera in the two execution paths,
// with nothing to notice.
//
// This header is the ONE table. It names all eighteen modes, says what each one follows, and gives
// each its guest entry point. The two dispatchers read it; neither owns the policy any more.
//
// THE NAMES, and where they stop. Eleven of the eighteen say what the mode DOES to the camera, read
// off the body each arm runs. Four do not, and are named for the ONE store they perform rather than
// for an effect that would be a guess:
//   * `kForceModeByte` (13) stores the value 6 into the mode byte and does nothing else this frame.
//     Mode 6 itself is "freeze the camera height at the master's", so calling 13 "freeze" would be
//     reading a consequence into a single store; the store is what is certain.
//   * `kTailOnly` (16) has no body at all — the camera driver still runs its post-mode tail after it.
//   * the four overlay modes (9, 10, 17, and the render-mode-keyed prologue) name the OVERLAY they
//     dispatch into, not what that overlay does, because the overlay's body is a guest leaf this
//     repository has not read.
// WHAT THE MODES ARE NOT. These are the driver mode byte `cam[0x64] & 0x3F`, which is not the same
// thing as the RENDER MODE byte at 0x800BF870, and not the same as the init-time mode selector,
// which maps a render mode onto a driver mode. Three different selectors, three different names here
// and in `camera_render_mode.h`.
#pragma once
#include <cstddef>
#include <cstdint>
#include <iterator>

namespace tomba::camera {

// The eighteen driver modes the guest's jump table at 0x80016A44 holds (18 uint32 entries, read out
// of MAIN.EXE). The table's own comment records that read; this enum is its type.
enum class CameraMode : uint8_t {
  kMainFollow = 0,           // the smoothing follow, then a render-mode-keyed overlay handler
  kTrackFollow = 1,          // track X/Z and Y, then the same render-mode-keyed prologue
  kSnapFollowScriptedA = 2,  // snap to a target, then the scripted look-build A
  kPitchFollow = 3,          // smooth the vertical look, then snap, then build the view
  kSnapFollowScriptedB = 4,  // snap to a target, then the scripted look-build B
  kSnapFollowMaster = 5,     // snap to the master position
  kFreezeAtMasterHeight = 6, // hold the camera at the master's height
  kSnapFollowSelf = 7,       // snap to the camera's own follow target
  kSimpleFollowSelf = 8,     // track X/Z then Y, no distance solve
  kFieldOverlay9 = 9,        // a field-overlay handler
  kAreaOverlayScripted = 10, // the A00 scripted-camera state machine
  kReset = 11,               // clear the mode byte and two sub-state bytes
  kForceModeByte = 12,       // as kReset: the guest's table gives 11 and 12 the same body
  kForceModeSix = 13,        // store 6 into the mode byte and stop
  kSnapFollowSelfAlias = 14, // the guest's table gives 7 and 14 the same body
  kSimpleFollowMaster = 15,  // the simple follow, against the master position
  kTailOnly = 16,            // no body; the driver's post-mode tail still runs
  kFieldOverlay17 = 17,      // a field-overlay handler
};

// The highest mode the driver's own `mode < 18` test admits. 17 is the last live value.
inline constexpr uint8_t kHighestCameraMode = 17u;

// The two modes that share a body in the guest's table, and the two that share another. Written down
// so a reader does not have to diff two switches to learn that 7/14 and 11/12 are aliases.
inline constexpr uint8_t kSnapFollowSelfAliasOf = 7u;
inline constexpr uint8_t kResetAliasOf = 11u;
inline constexpr uint8_t kForceModeSixValue = 6u;

// What a mode FOLLOWS, when it follows something. `kFollowsNone` is the honest answer for the modes
// that do not follow a target at all — the overlay dispatchers, the reset, the single-store modes and
// the tail-only mode — and it is why this is an enum rather than a bare address: a zero here would
// have read as "follows the master position", which is a different camera.
enum class FollowTarget : uint8_t {
  kFollowsNone,          // this mode does not move the camera toward a target
  kFollowsCameraSelf,    // the camera's own follow triple at cam+0x38
  kFollowsMaster,        // the master position in the global block (G+0x2C)
  kFollowsOverlay,       // a guest leaf in a loaded overlay owns the whole mode
  kFollowsRenderOverlay, // a render-mode-keyed overlay owns it, after the native follow runs
};

struct ModeDescriptor {
  CameraMode mode;
  FollowTarget follows;
  // The guest function entry the mode's body lives at, for the modes whose body is a single guest
  // leaf. Zero for a mode whose body is a sequence of already-owned native methods, because there is
  // no single entry to name.
  uint32_t guestEntry;
};

// The one table. Read it instead of either switch.
//
// `kMainFollow` and `kTrackFollow` carry no guest entry because their bodies are the already-owned
// `mainFollow` / `trackFollow` orchestrators followed by an indirect render-mode handler; every
// other entry with a non-zero `guestEntry` is a mode whose body this class does not own.
constexpr ModeDescriptor kCameraModes[] = {
    {CameraMode::kMainFollow, FollowTarget::kFollowsRenderOverlay, 0x8006E0F0u},
    {CameraMode::kTrackFollow, FollowTarget::kFollowsCameraSelf, 0x8006E228u},
    {CameraMode::kSnapFollowScriptedA, FollowTarget::kFollowsCameraSelf, 0x8006E294u},
    {CameraMode::kPitchFollow, FollowTarget::kFollowsCameraSelf, 0x8006E360u},
    {CameraMode::kSnapFollowScriptedB, FollowTarget::kFollowsCameraSelf, 0x8006E2FCu},
    {CameraMode::kSnapFollowMaster, FollowTarget::kFollowsMaster, 0x8006E3B0u},
    {CameraMode::kFreezeAtMasterHeight, FollowTarget::kFollowsNone, 0u},
    {CameraMode::kSnapFollowSelf, FollowTarget::kFollowsCameraSelf, 0x8006E3B0u},
    {CameraMode::kSimpleFollowSelf, FollowTarget::kFollowsCameraSelf, 0x8006E3F4u},
    {CameraMode::kFieldOverlay9, FollowTarget::kFollowsOverlay, 0x8018B924u},
    {CameraMode::kAreaOverlayScripted, FollowTarget::kFollowsOverlay, 0x8010D89Cu},
    {CameraMode::kReset, FollowTarget::kFollowsNone, 0u},
    {CameraMode::kForceModeByte, FollowTarget::kFollowsNone, 0u},
    {CameraMode::kForceModeSix, FollowTarget::kFollowsNone, 0u},
    {CameraMode::kSnapFollowSelfAlias, FollowTarget::kFollowsCameraSelf, 0x8006E3B0u},
    {CameraMode::kSimpleFollowMaster, FollowTarget::kFollowsMaster, 0x8006E3F4u},
    {CameraMode::kTailOnly, FollowTarget::kFollowsNone, 0u},
    {CameraMode::kFieldOverlay17, FollowTarget::kFollowsOverlay, 0x80111AB4u},
};

// The mode's descriptor, or nullptr for a mode value outside the eighteen. Reads the table by the
// mode's own value rather than by an ordinal, so a mode added out of order cannot silently pick up
// another mode's row.
constexpr const ModeDescriptor *descriptorFor(CameraMode mode) {
  const auto value = static_cast<uint8_t>(mode);
  for (const ModeDescriptor &row : kCameraModes) {
    if (static_cast<uint8_t>(row.mode) == value) {
      return &row;
    }
  }
  return nullptr;
}

// The guest function entry a mode's body lives at, or 0 for a mode whose body is a sequence of
// already-owned native methods. The four overlay modes and the follow modes are the only ones with
// a single entry; the reset, freeze and single-store modes have no body to name.
constexpr uint32_t guestEntryFor(CameraMode mode) {
  return descriptorFor(mode) != nullptr ? descriptorFor(mode)->guestEntry : 0u;
}

// The render-mode-keyed PROLOGUE that modes 0 and 1 share: three render modes have a dedicated
// overlay handler that runs INSTEAD of the mode's own follow, and every other render mode falls
// through to it. The guest's own dispatch is three equality tests in a fixed order; because the three
// values are distinct, the order cannot change which one matches, and this table is the single place
// the addresses live.
//
// The four return constants per row are the guest's OWN post-jump addresses, and they are per
// (mode, render mode) PAIR, not per mode: the guest emits a separate jump for each of the six
// combinations, and a body that pushes a frame writes its return address into guest stack bytes the
// reference compares. One constant per mode would therefore be a behaviour change, not a tidy-up —
// the native path does not need any of them because it reaches these leaves as C++ calls, and the
// guest-faithful path needs all six.
struct RenderModePrologue {
  uint8_t renderMode;
  uint32_t forMainFollow;     // the overlay entry mode 0 dispatches into
  uint32_t forTrackFollow;    // the overlay entry mode 1 dispatches into
  uint32_t mainFollowReturn;  // the guest's post-jump address for (mode 0, this render mode)
  uint32_t trackFollowReturn; // ...and for (mode 1, this render mode)
};

constexpr RenderModePrologue kRenderModePrologues[] = {
    {2u, 0x80115F58u, 0x80116918u, 0x8006ED38u, 0x8006EE0Cu},
    {7u, 0x80112DECu, 0x80113660u, 0x8006ED48u, 0x8006EDFCu},
    {20u, 0x8010AD0Cu, 0x8010B2F0u, 0x8006ED58u, 0x8006EE1Cu},
};
inline constexpr size_t kRenderModePrologues_count = std::size(kRenderModePrologues);

// The mode-0 tail: after its own follow, mode 0 also runs whichever handler the resident
// render-mode function-pointer table at 0x800A4AA0 holds for the current render mode.
inline constexpr uint32_t kRenderModeFunctionTable = 0x800A4AA0u;

// The resident render-mode BYTE itself. It is a different selector from the driver mode byte above
// and from the init-time selector, and the three are easy to confuse because all three are small
// integers indexing tables.
inline constexpr uint32_t kRenderModeByte = 0x800BF870u;

// ---- the per-render-mode camera-height FLOORS ----------------------------------------------------
//
// `yFloor` clamps the camera's height, and which clamp applies is chosen by the RENDER MODE byte. The
// five render modes below have one; the other eight impose none. The thresholds live here because
// they are the entire content of that function and a reader cannot otherwise tell a FLOOR from the
// COMPARISON BOUND next to it — several of the pairs differ by exactly one, and that difference is
// the guest's own (a "clamp unless already at or above the bound" test, which is not the same as a
// "clamp when below" test at the boundary). Spelling them apart is the point.
//
// The `kFloor*Index` constants are `renderMode - 1`, which is the index the guest's switch takes.
// Render mode 1 therefore has index 0, and render modes 0 and 14-and-above fall out of the range
// check before any of these are consulted.
inline constexpr uint32_t kFloorRenderModeIndex = 0u;         // render mode 1
inline constexpr uint32_t kFloorSeaRenderModeIndex = 3u;      // render mode 4
inline constexpr uint32_t kFloorShoreRenderModeIndex = 5u;    // render mode 6
inline constexpr uint32_t kFloorNightRenderModeIndex = 9u;    // render mode 10
inline constexpr uint32_t kFloorFinalRenderModeIndex = 12u;   // render mode 13
inline constexpr uint32_t kHighestFloorRenderModeIndex = 12u; // index 13 and above has no floor

inline constexpr int32_t kFloorRenderMode1 = -10140;
inline constexpr int32_t kFloorRenderMode10 = -2160;
inline constexpr int32_t kFloorRenderMode13Bound = -1399; // raise to kFloorRenderMode13 unless below
inline constexpr int32_t kFloorRenderMode13 = -1400;

// Render mode 4's two floors, selected by a sub-area byte at 0x800BF871. WHAT that byte selects is
// not established; the two arms are named for the floor each one produces, which is what is certain.
inline constexpr uint8_t kSeaSubAreaSeven = 7u;
inline constexpr int32_t kSeaDistanceBound = 6800;    // a distance read at 0x800E7EB6 gates both arms
inline constexpr int32_t kSeaFloorLow = -7299;        // at or below this, the arm below decides
inline constexpr int32_t kSeaFloorLowPlusOne = -6499; // the near arm's own lower bound
inline constexpr int32_t kSeaFloorNear = -6500;
inline constexpr int32_t kSeaFloorHighBound = -6599; // the far arm's lower bound
inline constexpr int32_t kSeaFloorHigh = -6600;

// Render mode 6's two floors, selected by the status byte the script interpreter's mirror leaf
// writes. That is the SAME guest word as `script_globals::kStatusByteMirror`, and this header names
// it by its own meaning here because the camera reads it as a sub-area selector and the script
// interpreter's leaf does not know it is one — the script leaf's header says the byte's meaning is
// not established, and that note still stands; what is established is that the camera branches on
// the value 14.
inline constexpr uint8_t kShoreSubAreaFourteen = 14u;
inline constexpr int32_t kShoreFloorNear = -7200;
inline constexpr int32_t kShoreFloorFar = -9200;

// ---- the camera SHAKE state machine ---------------------------------------------------------------
//
// The post-mode tail runs every frame after every mode. Its state byte is cam[0x76], written by
// EXTERNAL code and read here; state 0 is idle. Two families, both "capture an anchor, then jitter
// around it", differing in which axes move and whether the jitter repeats or fires once.
enum class CameraShakeState : uint8_t {
  kIdle = 0,                  // nothing to do
  kCaptureAxes = 1,           // snapshot the look position, then jitter on every frame
  kJitterAxes = 2,            // free-running X/Y/Z jitter around the captured anchor
  kRestoreAxes = 3,           // external code asks to stop: put the anchor back exactly, then go idle
  kCaptureHeight = 4,         // snapshot the height only, then jitter
  kJitterHeight = 5,          // free-running Y-only jitter
  kPulseHeightBegin = 6,      // one-shot: snapshot, then fall straight into the jitter IN THE SAME FRAME
  kPulseHeight = 7,           // the one-shot's jitter, ±32, then always go idle
  kPulseHeightSmallBegin = 8, // as 6, for the smaller ±16 pulse
  kPulseHeightSmall = 9,      // the ±16 pulse, then always go idle
};

// States 6/8 falling straight into 7/9 in the same frame is the guest's own control flow, not a bug,
// and it is why those two are separate members rather than one state with a flag.
inline constexpr uint8_t kHighestShakeState = 9u;
static_assert(static_cast<uint8_t>(CameraShakeState::kPulseHeightSmall) == kHighestShakeState,
              "the shake tail's `state >= 10 is not a shake state` guard must match the last member");

// The byte that aborts a one-shot pulse before it fires. Non-zero means the camera is busy elsewhere
// this frame, and the pulse is dropped rather than queued.
inline constexpr uint32_t kShakeBusyByte = 0x64u;

// The three anchor words: the captured look position the jitter is measured against, in the order
// the capture arm stores them.
inline constexpr uint32_t kShakeAnchorX = 0x86u;
inline constexpr uint32_t kShakeAnchorY = 0x88u;
inline constexpr uint32_t kShakeAnchorZ = 0x8au;

// The three jitter amplitudes, the masks each draw from, and the two effect ids the jitter queues.
// X and Z jitter by ±16, Y by ±8 in the three-axis family; the height-only families use their own
// amplitudes, which is why the amplitudes are per family rather than one constant.
inline constexpr uint32_t kShakeMaskWide = 0x3fu;         // 6 bits: the height-only jitters
inline constexpr uint32_t kShakeMaskNarrow = 0x1fu;       // 5 bits: the axes, and the small pulse
inline constexpr int32_t kShakeXAmplitude = 16;           // the X/Z jitter
inline constexpr int32_t kShakeYAmplitude = 8;            // the three-axis Y jitter — HALF the X/Z one
inline constexpr int32_t kShakeHeightAmplitude = 32;      // the free-running and ±32 height jitters
inline constexpr int32_t kShakeHeightSmallAmplitude = 16; // the ±16 height pulse
inline constexpr uint32_t kShakeEffectAxes = 129u;        // the effect id the axis jitter queues
inline constexpr uint32_t kShakeEffectHeight = 241u;      // ...and the one the Y-only jitter queues
inline constexpr uint32_t kShakeEffectPriority = 2u;

// The settled-bit mask the follow arms raise, and the two the scripted look-builds raise. NOT raised
// by the shake tail: a shake displaces the look position without the camera having settled onto it.
inline constexpr uint8_t kSettledAxesBit = 1u;
inline constexpr uint8_t kSettledHeightBit = 2u;

} // namespace tomba::camera
