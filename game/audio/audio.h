// audio.h — Tomba! 2's native music mixed over the SPU, and the Sound Test.
#pragma once

#include "game_runtime.h"

#include <cstdint>
#include <string_view>

namespace tomba {

class MusicList;
class NativeMusic;

} // namespace tomba

namespace tomba {

class Audio final : public psx::game::TitleAudio {
public:
  Audio(NativeMusic &music, MusicList &tracks) : music_(music), tracks_(tracks) {}

  void mixFrame(std::int16_t *pcm, int frames) override;
  std::string_view nowPlayingName() const override;
  int trackCount() const override;
  std::string_view trackName(int track) const override;
  void soundTestPlay(int track) override;

private:
  NativeMusic &music_;
  MusicList &tracks_;
};

} // namespace tomba
