#pragma once

#include <cstdint>
#include <string>

class Core;

namespace tomba {

// The control-channel `warp <area> [entry]` request. It writes the destination words and the pending-transition
// byte that the game's own field machine consumes (the pair the community debug menu's WARP TOOLS page writes).
// Armed between frames, applied at the next frame boundary.
//
// The field run (area machines 2 and 3) takes any non-zero pending byte; the GAME-image handler of machine 4 takes 1
// as a door and sends other values to the game-over menu, and machines 5 and 6 take only 3. The code written is the
// one the running machine accepts.
class DevWarp {
public:
  // 0x800BF839: the field machine's pending-transition request; 0 none.
  inline static constexpr uint32_t kPendingTransition = 0x800bf839u;
  // 0x800BF83A: u16, area in the high byte and entry in the low byte.
  inline static constexpr uint32_t kDestination = 0x800bf83au;
  inline static constexpr uint8_t kDoorRequest = 1;
  inline static constexpr uint8_t kHandlerExitRequest = 3;
  // 0x800BF89C: 2 while the scripted opening runs, 4 in ordinary play.
  inline static constexpr uint32_t kLoadMode = 0x800bf89cu;
  inline static constexpr uint8_t kScriptedOpening = 2;
  inline static constexpr uint8_t kOrdinaryPlay = 4;
  inline static constexpr uint32_t kMaxEntry = 0x3f;
  // Areas 0..21: one A0* overlay each on the disc.
  inline static constexpr int kAreaCount = 22;

  // Where the field machine is, as far as a request is concerned.
  enum class Phase : uint8_t {
    Unavailable, // not in a state that consumes a request
    Running,     // field running: the machine reads the request on its next pass
    Opening,     // scripted opening: left the way the Start skip leaves it
  };

  static Phase phase(Core &core);

  // Parses and arms one command line; the returned text answers the client.
  std::string arm(Core &core, const char *line);
  // Writes the armed request at a frame boundary.
  void applyArmed(Core &core, uint32_t frame);

private:
  bool armed_ = false;
  uint32_t area_ = 0;
  uint32_t entry_ = 0;
};

} // namespace tomba
