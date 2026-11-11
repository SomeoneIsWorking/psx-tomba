#pragma once

#include <cstdint>
#include <string>

class Core;

namespace tomba {

// The camera re-place an area event ends with (A06 FUN_80126334, FUN_80131DD8, A00 FUN_80128D04 ...): the camera
// snapped to Tomba by CutsceneCamera::kSnapToMaster and its mode byte cleared. Armed between frames, applied at the
// next frame boundary, so a declared cut can be reached without playing the event.
class DevCameraPlace {
public:
  // The returned text answers the client.
  std::string arm();
  void applyArmed(Core &core, std::uint32_t frame);

private:
  bool armed_ = false;
};

} // namespace tomba
