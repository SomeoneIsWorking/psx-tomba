#pragma once

#include <cstdint>
#include <string>

class Core;

namespace tomba {

// The debug menu's `subscreen <item>`: opens the field subscreen (page 6) as the page step's own field arm does,
// then, once it runs, enters the Items tab with the cursor on `item`. Armed between frames, applied at frame
// boundaries.
class DevSubscreen {
public:
  // The returned text answers the client.
  std::string arm(std::uint32_t item);
  void applyArmed(Core &core, std::uint32_t frame);

private:
  enum class Step : std::uint8_t { None, Open, Enter };

  Step step_ = Step::None;
  std::uint32_t item_ = 0;
  std::uint32_t armedAt_ = 0;
};

} // namespace tomba
