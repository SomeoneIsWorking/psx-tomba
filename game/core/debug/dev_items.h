#pragma once

#include "items/inventory.h"

#include <cstdint>
#include <string>

class Core;

namespace tomba {

// The debug menu's `items-all` and `item <id> [amount]` requests. `all` is the community debug menu's
// GRANT ALL ITEMS; an id goes through the game's own give-and-flag. Armed between frames, applied at the next
// frame boundary.
class DevItems {
public:
  inline static constexpr uint32_t kMaxItemId = 255;
  inline static constexpr uint32_t kMaxAmount = 99;

  // Each returns the reply; the debug menu checked the ranges and that the GAME field runs.
  std::string armAll();
  std::string armOne(uint32_t item, uint32_t amount);
  // Gives the game's own give-and-flag one item; the frame driver supplies the shipping inventory.
  using GiveFn = void (*)(Core &core, const Inventory::ItemGrantArgs &grant);
  void applyArmed(Core &core, GiveFn give, uint32_t frame);

private:
  enum class Request : uint8_t { None, All, One };

  Request request_ = Request::None;
  uint32_t item_ = 0;
  uint32_t amount_ = 0;
};

} // namespace tomba
