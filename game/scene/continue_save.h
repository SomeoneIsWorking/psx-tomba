// game/scene/continue_save.h — the title's Continue: finds the newest save on the card and loads it.
#pragma once

#include <cstdint>
#include <optional>

class Core;

namespace tomba::scene {

class ContinueSave {
public:
  // The file number NN of the `BASCUSP94454T2-NN` written last (Memcard::writtenAt), else played longest.
  static std::optional<std::uint8_t> newest(Core &core);
  // Loads file `number` into the state image (0x800BF870) through CRD's own load ops; false if the game refused it.
  static bool load(Core &core, std::uint8_t number);
};

} // namespace tomba::scene
