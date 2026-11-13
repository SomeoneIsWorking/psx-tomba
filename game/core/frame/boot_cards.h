#pragma once

#include "blob_codec.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class Core;

namespace tomba {

// The retail boot's publisher cards, one host turn each: the SCEA license screen the disc's boot stub
// SCUS_944.54 draws, then the LOGO.STR movie. Both finish before the game's own frames begin.
class BootCards {
public:
  enum class Phase : uint8_t { Scea, Movie, Done };
  // What a machine state carries: the card on screen and, on the SCEA card, its frame.
  struct Progress {
    Phase phase = Phase::Scea;
    int sceaFrame = 0;
  };

  bool finished() const {
    return phase_ == Phase::Done;
  }
  // Presents one card frame and commits it as an unpresented field.
  void step(Core &core);

  // Refuses during the movie: its decoder is host state.
  bool save(psx::platform::BlobWriter &out, std::string &error) const;
  static std::optional<Progress> read(psx::platform::BlobReader &in, std::string &error);
  void resume(const Progress &progress);

private:
  void stepScea(Core &core);
  void stepMovie(Core &core);
  void finish(Core &core);

  Phase phase_ = Phase::Scea;
  int sceaFrame_ = 0;
  bool movieOpen_ = false;
  std::vector<uint8_t> sceaImage_;
};

} // namespace tomba
