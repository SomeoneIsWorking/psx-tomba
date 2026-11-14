// dev_life_loss.h — Tomba! 1's debug route to a life loss, through the game's own damage path.
#pragma once

#include <cstdint>
#include <string>

class Core;

namespace tomba1 {

// Armed between frames and applied at the next frame boundary as a lethal hit, written the way the game's own
// damage sites write one (playerHandleHazardContact x00 0x800FE970, its area 14 kill arm): decreaseObjectTimer
// 0x800505B0 by the player's whole vitality, then the hurt mode (+0x00 active 2, +0x04 mode 2, +0x05 step 0
// knockback, +0x06 state 0). From there the game runs it: the knockback sees vitality 0 and raises the hurt state
// 0x8009BCA0 to 1, the die step 0x800FDABC raises it to 2, and the field machine 0x8001B2B4 state 2 takes the
// life (0x8009BCE8) and enters the respawn, machine 0 0x8001B0A4 state 5.
class DevLifeLoss {
public:
  // The player object (TObj, 0xC0 bytes) and its words the hit writes.
  inline static constexpr std::uint32_t kPlayer = 0x800A5398u;
  inline static constexpr std::uint32_t kActive = 0x00u;
  inline static constexpr std::uint32_t kMode = 0x04u;
  inline static constexpr std::uint32_t kStep = 0x05u;
  inline static constexpr std::uint32_t kState = 0x06u;
  inline static constexpr std::uint32_t kContactKind = 0xA1u;
  inline static constexpr std::uint32_t kContactCD = 0xCDu;
  inline static constexpr std::uint32_t kContactCE = 0xCEu;
  inline static constexpr std::uint32_t kVitality = 0x98u;
  // Active 1 is the only state a damage site hits; mode 2 step 0 is playerHurtModeDispatch's knockback.
  inline static constexpr std::uint8_t kActiveFree = 1u;
  inline static constexpr std::uint8_t kActiveHurt = 2u;
  inline static constexpr std::uint8_t kModeHurt = 2u;
  inline static constexpr std::uint32_t kDecreaseObjectTimer = 0x800505B0u;
  // The field machine 0x8001B2B4 is the only one that reads the hurt state.
  inline static constexpr std::uint16_t kFieldMachine = 1u;
  inline static constexpr std::uint32_t kLives = 0x8009BCE8u;

  // Empty when a hit may land now; otherwise why not.
  static std::string refusal(Core &core);
  std::string arm(Core &core);
  void applyArmed(Core &core, std::uint32_t frame);

private:
  bool armed_ = false;
};

} // namespace tomba1
