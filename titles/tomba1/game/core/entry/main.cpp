#include "cfg.h"
#include "core.h"
#include "dbg_server.h"
#include "frame_loop_shell.h"
#include "game.h"
#include "hw_bind.h"
#include "psx_exe_image.h"
#include "stream_field_turn.h"
#include "tomba1_runtime.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <lucent/log.h>
#include <memory>
#include <string_view>

extern "C" {
void mdec_init();
void spu_init();
void watchdog_init();
}

namespace {

constexpr const char *kDefaultExecutable = "scratch/bin/tomba1/SCUS_942.36";
constexpr const char *kDiscEnvironmentKey = "PSXPORT_TOMBA1_DISC";

// The unattended-run frame cap, read from PSXPORT_NATIVE_FRAMES. 0 means "no cap", which is the
// right answer for a windowed or client-driven run. This is a TITLE-side read of a launch
// argument, not configuration the product subsystem owns: `DbgServer::attach` is what decides
// whether that cap actually applies, and this only supplies the requested value.
int nativeFrameCap() {
  const char *value = cfg_str("PSXPORT_NATIVE_FRAMES");
  if (!value || !*value) {
    return 0;
  }
  char *end = nullptr;
  const long parsed = std::strtol(value, &end, 0);
  if (end == value || parsed <= 0 || parsed > 1000000L) {
    lucent::error("boot", "PSXPORT_NATIVE_FRAMES={} is not a frame count in 1..1000000", value);
    return 2;
  }
  return static_cast<int>(parsed);
}

bool isHelpRequest(int argc, char **argv) {
  return argc == 2 && (std::string_view(argv[1]) == "-h" || std::string_view(argv[1]) == "--help");
}

void printUsage() {
  std::puts("Usage: tomba1_port\n"
            "Launch the provisioned Tomba! USA product using PSXPORT_TOMBA1_DISC.\n"
            "\n"
            "Options:\n"
            "  -h, --help  Show this help and exit");
}

} // namespace

int main(int argc, char **argv) {
  if (isHelpRequest(argc, argv)) {
    printUsage();
    return 0;
  }
  if (argc != 1) {
    lucent::error("boot", "tomba1_port takes no executable override; provision the verified Tomba! USA disc");
    return 2;
  }
  if (!std::filesystem::is_regular_file(kDefaultExecutable)) {
    lucent::error("boot", "{} is absent; run titles/tomba1/tools/provision.py first", kDefaultExecutable);
    return 2;
  }
  if (!cfg_str(kDiscEnvironmentKey)) {
    lucent::error("boot",
                  "{} is unset; select the verified Tomba! USA disc explicitly (generic PSXPORT_DISC and drop-in "
                  "fallbacks are refused)",
                  kDiscEnvironmentKey);
    return 2;
  }

  static tomba1::Tomba1Runtime runtime;
  psxport_install_game(runtime);

  auto game = std::make_unique<Game>();
  game->disc.env_key = kDiscEnvironmentKey;
  Core *core = &game->core;
  tomba1::registerStreamFieldTurn(*core);
  watchdog_init();
  load_exe(kDefaultExecutable, core);
  gte_init();
  mdec_init();
  spu_init();
  gte_bind(core);
  core->rsub.projprim.bind(core);
  spu_bind(core);
  mdec_bind(core);
  xa_bind(core);
  game->spu_audio.init();
  game->gpu.gpu_native_init();
  game->pad.overridesInit();
  core->runtime->registerOverrides(*game);

  psx::frame::FrameLoopShell shell;
  shell.prepareProduct(*game);
  // The product's control channel, always open on loopback. This title composes its own frame loop
  // rather than entering the framework's `native_boot` spine, so the channel has to be attached here:
  // without it a headless run has no way to be driven or captured at a chosen game state, which is
  // what a matched 4:3/wide comparison needs. It stays open under ./run.sh and under the bare
  // executable, so the player's own session is probeable too; the env var only moves the port.
  //
  // `attach` also answers the frame cap this loop owes, and answering it is not optional: 0 means
  // "until quit", which is right for a client-driven or windowed run and wrong for an unattended
  // headless one that would otherwise never return. The framework owns that decision, so this reads
  // its answer instead of repeating the policy.
  const int frameCap = game->dbg_server.attach(core, nativeFrameCap());
  for (std::uint32_t frame = 0; frameCap == 0 || static_cast<int>(frame) < frameCap; ++frame) {
    // The pause policy lives in the framework because a frozen game must not advance; this title's
    // loop owes the same behaviour, and a second copy of "what a pause does" would be free to
    // disagree with the other boot spines.
    game->dbg_server.honourPause(core);
    shell.step(*core, frame);
    game->dbg_server.service(core);
  }
  return 0;
}
