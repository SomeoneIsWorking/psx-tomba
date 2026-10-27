// audio.cpp — saturating mix of the native music over one video frame of SPU output.
#include "audio/audio.h"

#include "audio/music_list.h"
#include "audio/native_music.h"

#include <algorithm>
#include <array>

namespace tomba {

namespace {

constexpr int kMaxFrames = 735 + 64; // the SPU host sink's per-video-frame cap

} // namespace

void Audio::mixFrame(std::int16_t *pcm, int frames) {
  if (!music_.active()) {
    return;
  }
  frames = std::min(frames, kMaxFrames);
  std::array<std::int16_t, std::size_t{2} * kMaxFrames> music{};
  music_.render(music.data(), frames);
  for (int i = 0; i < frames * 2; ++i) {
    pcm[i] = static_cast<std::int16_t>(std::clamp(pcm[i] + music[i], -32768, 32767));
  }
}

std::string_view Audio::nowPlayingName() const {
  const int track = tracks_.nowPlaying();
  return track >= 0 ? trackName(track) : std::string_view{};
}

int Audio::trackCount() const {
  return tracks_.count();
}

std::string_view Audio::trackName(int track) const {
  const char *name = tracks_.name(track);
  return name != nullptr ? std::string_view(name) : std::string_view{};
}

void Audio::soundTestPlay(int track) {
  if (track < 0) {
    tracks_.stop();
  } else {
    tracks_.play(track);
  }
}

} // namespace tomba
