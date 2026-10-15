// engine_cd.h — Tomba! 2's CD overrides and the XA voice/BGM clip state they keep per session.
#pragma once

#include <lucent/log.h>

#include <array>
#include <cstdint>

class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba {

// The engine's own CD glue (FUN_8001Cxxx/FUN_8001Dxxx) and the libcd read primitive it builds on, as
// resident overrides. No CD interrupt is raised.
class EngineCd {
public:
  // An XA clip as FUN_8001D2A8 names it: channel and LBA span; a looping clip is area music.
  struct Clip {
    std::uint8_t channel = 0;
    std::uint32_t start = 0;
    std::uint32_t end = 0;
    bool loop = false;
  };
  // A looping area-music clip held back during a dialog tone and replayed after it.
  struct DeferredMusic {
    bool pending = false;
    std::uint8_t channel = 0;
    std::uint32_t start = 0;
    std::uint32_t end = 0;
  };

  explicit EngineCd(Core &core) : core_(core) {}

  // Declares the CD overrides in the resident catalog.
  static void declareOverrides(native::NativeOverrideCatalog &natives);

  // FUN_8001CF00(on): CD-to-SPU mixing; Beetle's SPU mixes XA only with it on.
  void toSpuMix(bool on);
  // FUN_800898A0's success path: the four CD event callbacks installed, no controller handshake.
  void hleInit();
  // The CD fade and XA lifecycle on change (`PSXPORT_DEBUG=cd_override`).
  void audioTrace(const char *tag);

  DeferredMusic &deferredMusic() {
    return deferredMusic_;
  }
  const DeferredMusic &deferredMusic() const {
    return deferredMusic_;
  }

  // Streams `clip` and holds voice task slot 2 for it; the clip already streaming is not restarted.
  void playClip(const Clip &clip);
  // Stops the stream and releases slot 2.
  void stopClip();
  // Slot 2 belongs to a clip until the scheduler sees it end.
  bool holdsVoiceSlot() const {
    return holdsVoiceSlot_;
  }
  bool clipStreaming() const;
  void releaseVoiceSlot() {
    holdsVoiceSlot_ = false;
  }

private:
  Core &core_;
  DeferredMusic deferredMusic_;
  Clip clip_;
  bool holdsVoiceSlot_ = false;
  // tgt cur mas 19a 137 song act loop gate
  std::array<int, 9> traced_{1 << 30};
  lucent::Channel traceChannel_{"cd_override"};
};

} // namespace tomba
