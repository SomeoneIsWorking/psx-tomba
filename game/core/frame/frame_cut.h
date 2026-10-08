// game/core/frame/frame_cut.h — whether a sealed frame record is a cut, for the 60 fps in-between.
//
// A record is a cut when the scene it shows is not the scene of the record before it: another stage
// (0x801FE00C), field area (0x800BF870), sub-scene (0x800BF871) or SOP intro shot (0x800BF9B4, each
// shot places the camera afresh), or a camera the guest re-placed at the master position
// (FUN_8006EA00) instead of following there. Identity is sampled when the pass that builds the
// record's ordering table ends, so it describes that record.
#pragma once

#include <cstdint>
#include <optional>

class Core;

namespace tomba {

class FrameCut {
public:
  inline static constexpr std::uint32_t kStagePointer = 0x801FE00Cu;
  inline static constexpr std::uint32_t kAreaByte = 0x800BF870u;
  inline static constexpr std::uint32_t kSubSceneByte = 0x800BF871u;
  inline static constexpr std::uint32_t kIntroShotByte = 0x800BF9B4u;

  // FUN_8006EA00 ran during the pass being built.
  void noteCameraPlaced() {
    cameraPlaced_ = true;
  }
  // The pass that built the next record has ended.
  void notePassEnded(Core &core);
  // Whether the most recently ended pass is a cut from the one before it.
  [[nodiscard]] bool isCut() const {
    return cut_;
  }

private:
  struct SceneIdentity {
    std::uint32_t stage = 0;
    std::uint8_t area = 0;
    std::uint8_t subScene = 0;
    std::uint8_t introShot = 0;
    bool operator==(const SceneIdentity &) const = default;
  };

  std::optional<SceneIdentity> previous_;
  bool cameraPlaced_ = false;
  bool cut_ = true;
};

} // namespace tomba
