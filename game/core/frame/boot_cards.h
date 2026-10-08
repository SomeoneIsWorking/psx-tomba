#pragma once

#include <cstdint>
#include <memory>
#include <vector>

class Core;

namespace tomba {

// The retail boot's publisher cards, one host turn each: the SCEA license screen the disc's boot stub
// SCUS_944.54 draws, then the LOGO.STR movie. Both finish before the game's own frames begin.
class BootCards {
public:
  bool finished() const {
    return phase_ == Phase::Done;
  }
  // Presents one card frame and commits it as an unpresented field.
  void step(Core &core);

private:
  enum class Phase : uint8_t { Scea, Movie, Done };

  void stepScea(Core &core);
  void stepMovie(Core &core);
  void finish(Core &core);

  Phase phase_ = Phase::Scea;
  int sceaFrame_ = 0;
  bool movieOpen_ = false;
  std::vector<uint8_t> sceaImage_;
};

} // namespace tomba
