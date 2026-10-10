#pragma once

#include <cstdint>
#include <string>

class Core;

namespace tomba {

// The control-channel `items all` and `items <id> [amount]` commands. `all` is the community debug menu's
// GRANT ALL ITEMS; an id goes through the game's own give-and-flag. Armed between frames, applied at the next
// frame boundary.
class DevItems {
public:
  inline static constexpr uint32_t kMaxItemId = 255;
  inline static constexpr uint32_t kMaxAmount = 99;

  std::string arm(Core &core, const char *line);
  // Gives the game's own give-and-flag one item; the frame driver supplies the shipping inventory.
  using GiveFn = void (*)(Core &core, uint32_t item, uint32_t amount);
  void applyArmed(Core &core, GiveFn give, uint32_t frame);

private:
  enum class Request : uint8_t { None, All, One };

  Request request_ = Request::None;
  uint32_t item_ = 0;
  uint32_t amount_ = 0;
};

} // namespace tomba
