// tomba_audio.h — Tomba! 2's native music mixed over the SPU, and the Sound Test.
#pragma once

#include "game_runtime.h"

#include <cstdint>

namespace tomba {

class MusicList;
class NativeMusic;

} // namespace tomba

namespace tomba {

class TombaAudio final : public psx::game::TitleAudio {
public:
  TombaAudio(NativeMusic &music, MusicList &tracks) : music_(music), tracks_(tracks) {}

  void mixFrame(std::int16_t *pcm, int frames) override;
  const char *nowPlayingName() const override;
  void soundTestPlay(int track) override;

private:
  NativeMusic &music_;
  MusicList &tracks_;
};

} // namespace tomba
