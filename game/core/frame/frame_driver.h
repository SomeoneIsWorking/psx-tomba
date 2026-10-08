#pragma once

#include "debug/auto_drive.h"
#include "frame/frame_diagnostics.h"
#include "game_runtime.h"

#include <cstdint>

class Game;

namespace tomba {

// FUN_80050B08's loop state and the libgpu entry it alone calls (docs/re/frame-loop.md).
inline constexpr uint32_t kBufferParityAddress = 0x1F800135u; // u8, 0 or 1
inline constexpr uint32_t kPresentStateAddress = 0x1F80019Cu; // u8, see PresentState
inline constexpr uint32_t kPutDispEnv = 0x8008179Cu;
inline constexpr uint32_t kOtEntries = 0x800u;
inline constexpr uint32_t kDispEnvOffset = 0x2000u;
inline constexpr uint32_t kDrawEnvOffset = 0x2014u;
inline constexpr uint32_t kOtHeadOffset = 0x1FFCu;

// What a loop pass does after its vblank gate, keyed by kPresentStateAddress.
enum class PresentState : uint8_t {
  Present = 0, // PutDispEnv, PutDrawEnv, DrawOTag, flip, ClearOTagR the new buffer
  Hold = 1,    // nothing
  Swap = 2,    // PutDispEnv, state = Hold, flip
  Clear = 3,   // ClearOTagR the current buffer
};

// One pass of Tomba! 2's retail main loop (FUN_80050B08) per logic frame; the framework repeats it.
class TombaFrameDriver final : public FrameDriver {
public:
  explicit TombaFrameDriver(Game &game);

  // The loop prologue at 0x80050C50: first packet pool, current buffer, its OT cleared.
  static void enterLoop(Core &core);

  void stepFrame(Core &core, uint32_t frame) override;

private:
  Game *game_;
  AutoDrive autoDrive_;
  FrameDiagnostics diagnostics_;
};

} // namespace tomba
