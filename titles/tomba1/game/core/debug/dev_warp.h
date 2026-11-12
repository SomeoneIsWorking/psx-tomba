#pragma once

#include <cstdint>
#include <string>

class Core;

namespace tomba1 {

// The warp catalogue's area-section arrival. Armed between frames and applied at the next frame
// boundary the way a door does it (0x8002CFFC): the destination words, stopBgm(1) when the area changes, then the
// area machine to the transition handler 0x8001CFCC, which fades, loads and enters the destination.
class DevWarp {
public:
  // 0x1F8001D4: the gameplay task's record; +0x4C is the area machine, +0x4E its state.
  inline static constexpr std::uint32_t kTaskRecordPointer = 0x1F8001D4u;
  inline static constexpr std::uint32_t kMachineOffset = 0x4Cu;
  inline static constexpr std::uint32_t kStateOffset = 0x4Eu;
  // The machines a door leaves through machine 7 (0x8002D408: 1, 2 and 4..6), each ticking in state 1.
  inline static constexpr std::uint16_t kFieldRunning = 1;
  inline static constexpr std::uint16_t kTransitionMachine = 7;
  // The destination 0x8001CFCC state 3 loads: u16 area, u16 section, u16 entry.
  inline static constexpr std::uint32_t kDestinationArea = 0x8009C0FCu;
  inline static constexpr std::uint32_t kDestinationSection = 0x8009C0FEu;
  inline static constexpr std::uint32_t kDestinationEntry = 0x8009C108u;
  inline static constexpr std::uint32_t kCurrentArea = 0x8009BCC8u;
  // The debug area select's bounds (0x8001AD58): the last area, and each area's section count.
  inline static constexpr std::uint32_t kLastArea = 0x8007B290u;
  inline static constexpr std::uint32_t kSectionCounts = 0x8007B294u;
  inline static constexpr std::uint32_t kStopBgm = 0x80020C00u;

  static bool fieldRunning(Core &core);
  static constexpr bool doorMachine(std::uint16_t machine) {
    return machine == 1u || machine == 2u || (machine >= 4u && machine <= 6u);
  }
  // Areas 0..last, from the debug area select's bound.
  static int areaCount(Core &core);

  // Arms the request, refused when the area has no such section; the returned text answers the client.
  std::string arm(Core &core, std::uint32_t area, std::uint32_t section, std::uint32_t entry);
  // Writes the armed request at a frame boundary.
  void applyArmed(Core &core, std::uint32_t frame);

private:
  bool armed_ = false;
  std::uint32_t area_ = 0;
  std::uint32_t section_ = 0;
  std::uint32_t entry_ = 0;
};

} // namespace tomba1
