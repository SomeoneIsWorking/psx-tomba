// game/audio/song_switch.h — class SongSwitch, the sequenced-song switcher FUN_80074BF8 and its stop FUN_80074E48.
#pragma once

#include <cstdint>
#include <optional>

class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba {

class SongSwitch {
public:
  Core *core = nullptr;

  // FUN_80074BF8(song): play song (low 7 bits; bit 7 marks a resumption, looped). A menu sting over a dialog tone
  // parks the tone in kResumeSong; AreaSlots::updateTail requests it back once the sting ends. Port difference: the
  // parked tone is paused and replayed where it stopped, where retail stops it and plays it from the start.
  void play(std::uint32_t song);
  // FUN_80074E48: stop the current song and release its slots.
  void stop();

  void registerOverrides(native::NativeOverrideCatalog &natives);

private:
  // Stop the current song, or with keepPaused hold it paused where it is.
  void release(bool keepPaused);

  std::optional<std::uint16_t> paused_; // the parked tone held paused under a sting
};

} // namespace tomba
