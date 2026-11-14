#pragma once

#include "world/area_entries.h"

#include <cstdint>
#include <optional>
#include <string>

class Core;

namespace tomba {

// The warp catalogue's area-entry arrival. It writes the destination words and the pending-transition
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
  // The field run (machine 2) enters GAME sub-mode 3, the ending, on 8.
  inline static constexpr uint8_t kEndingRequest = 8;
  // 0x800BF89C: 2 while the scripted opening runs, 4 in ordinary play.
  inline static constexpr uint32_t kLoadMode = 0x800bf89cu;
  inline static constexpr uint8_t kScriptedOpening = 2;
  inline static constexpr uint8_t kOrdinaryPlay = 4;
  // 0x1F800134: set, Tomba's spawn FUN_800783DC places him at the saved position instead of the entry record.
  inline static constexpr uint32_t kSpawnAtSaved = 0x1f800134u;
  // 0x800BF890/94/98: the saved position (16.16 x, y, z) a save point writes; 0x800BFE38 its collision sector.
  inline static constexpr uint32_t kSavedPosition = 0x800bf890u;
  inline static constexpr uint32_t kSavedSector = 0x800bfe38u;
  // Where the field machine is, as far as a request is concerned.
  enum class Phase : uint8_t {
    Unavailable, // not in a state that consumes a request
    Running,     // field running: the machine reads the request on its next pass
    Opening,     // scripted opening: left the way the Start skip leaves it
  };

  static Phase phase(Core &core);

  struct Destination {
    uint32_t area = 0;
    uint32_t entry = 0;
  };
  // A position on the area's collision grid: its sector and world coordinates.
  struct Spot {
    uint8_t sector = 0;
    int16_t x = 0;
    int16_t y = 0;
    int16_t z = 0;
  };

  // Arms the request; the returned text answers the client. The debug menu checked the area and the phase; an entry
  // past the area's records (`AreaEntries::count`) is refused.
  std::string arm(Core &core, Destination destination);
  // 0x1F800236: the scene-transition type FieldTransition::step dispatches on. 1-4 the door corridor
  // (FieldTransition::main), 5 and 6 Tomba's own exits, 7 an area object's exit, 8 a boss arena's clear.
  inline static constexpr uint32_t kTransitionType = 0x1f800236u;
  inline static constexpr uint8_t kDoorTransition = 1;
  inline static constexpr uint8_t kBossClearTransition = 8;
  inline static constexpr uint8_t kLastTransition = 8;
  // As arm, through scene-transition `type` (kDoorTransition is the door's fade-out and fade-in).
  std::string armThrough(Core &core, Destination destination, uint8_t type);
  // As arm, with Tomba spawned at `spot` through the spawn's saved-position branch, the way a card load
  // resumes at a save point.
  std::string armAt(Core &core, Destination destination, Spot spot);
  // Arms the ending request, which only the field run (machine 2) consumes.
  std::string armEnding();
  // Writes the armed request at a frame boundary.
  void applyArmed(Core &core, uint32_t frame);

private:
  bool armed_ = false;
  std::optional<uint8_t> transition_;
  bool ending_ = false;
  std::optional<Spot> spot_;
  uint32_t area_ = 0;
  uint32_t entry_ = 0;
};

} // namespace tomba
