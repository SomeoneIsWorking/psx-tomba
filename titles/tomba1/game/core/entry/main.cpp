#include "cfg.h"
#include "core.h"
#include "dbg_server.h"
#include "frame_loop_shell.h"
#include "game.h"
#include "hw_bind.h"
#include "machine.h"
#include "psx_exe_image.h"
#include "run_budget.h"
#include "stream_field_turn.h"
#include "title_catalog.h"
#include "tomba1_runtime.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <lucent/log.h>
#include <memory>
#include <string>
#include <string_view>

extern "C" {
void mdec_init();
void spu_init();
void watchdog_init();
}

namespace {

constexpr psx::host::TitleIdentity kTitleIdentity{
    .displayName = "Tomba!",
    .serial = "SCUS_942.36",
    .slug = "tomba1",
    .discEnvVar = "PSXPORT_TOMBA1_DISC",
    .discExecutablePath = "SCUS_942.36",
};
constexpr const char *kProvisioningRoot = "scratch/bin";

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
  const std::string executable = kTitleIdentity.defaultExecutable(kProvisioningRoot).string();
  if (!std::filesystem::is_regular_file(executable)) {
    lucent::error("boot", "{} is absent; run titles/tomba1/tools/provision.py first", executable);
    return 2;
  }
  if (!cfg_str(kTitleIdentity.discEnvVar)) {
    lucent::error("boot",
                  "{} is unset; select the verified Tomba! USA disc explicitly (generic PSXPORT_DISC and drop-in "
                  "fallbacks are refused)",
                  kTitleIdentity.discEnvVar);
    return 2;
  }

  static tomba1::Tomba1Runtime runtime;
  psxport_install_game(runtime);

  auto game = std::make_unique<Game>();
  psx::Machine{*game}.insertDisc(kTitleIdentity);
  Core *core = &game->core;
  tomba1::registerStreamFieldTurn(*core);
  watchdog_init();
  load_exe(executable.c_str(), core);
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
  // The control channel, always open on loopback; this title owns its loop, so it attaches it here.
  game->dbg_server.start(core);
  // 0 means "until quit"; RunBudget decides, a live client included.
  const auto frameCap = static_cast<std::uint32_t>(psx::host::RunBudget::fromConfig().cap());
  for (std::uint32_t frame = 0; frameCap == 0 || frame < frameCap; ++frame) {
    // The pause policy lives in the framework because a frozen game must not advance; this title's
    // loop owes the same behaviour, and a second copy of "what a pause does" would be free to
    // disagree with the other boot spines.
    game->dbg_server.honourPause(core);
    shell.step(*core, frame);
    game->dbg_server.service(core);
  }
  return 0;
}
