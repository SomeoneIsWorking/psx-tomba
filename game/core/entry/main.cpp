// game/core/main.cpp — the Tomba!2 process entry point.
//
// main() composes the process-lifetime TombaRuntime and framework machine. Game-specific behavior
// is reached through inheritance; the framework provides no main().
#include "cfg.h"
#include "core.h"
#include "entry/tomba_runtime.h"
#include "fs_util.h" // Fs::exists — MAIN.EXE presence probe for self-provisioning below
#include "game.h"
#include "hw_bind.h"
#include "lightrec_executor.h"
#include "platform_hle.h" // class PlatformHle — HW-sync HLE table (VSync/CdSync/MDEC/ChangeThread)
#include "psx_exe_image.h"
#include <lucent/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// C subsystems (compiled as C) reached across the boundary — declare with C linkage.
extern "C" {
void watchdog_init(void);
void mdec_init(void);
void spu_init(void);
}

int main(int argc, char **argv) {
  if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
    printf("Usage: tomba2_port [MAIN.EXE]\n"
           "Launch the Tomba! 2 native PC product. If MAIN.EXE is omitted, the product uses or "
           "provisions scratch/bin/tomba2/MAIN.EXE.\n\n"
           "Options:\n"
           "  -h, --help  Show this help and exit\n");
    return 0;
  }

  // Installation precedes every Game so Core can bind the title runtime.
  static tomba::TombaRuntime runtime;
  psxport_install_game(runtime);
  const char *path = argc > 1 ? argv[1] : "scratch/bin/tomba2/MAIN.EXE";
  Game *game = new Game(); // the whole machine (owns the Core + every subsystem's state — no globals)
  Core *c = &game->core;   // the CPU/RAM handle threaded through the runtime (2 MB RAM lives in Game)
  // Self-provision MAIN.EXE: anyone with just a CHD (drop-in *.chd in the repo root, or
  // PSXPORT_TOMBA2_DISC / .env) can run the binary directly — no prior ./run.sh extraction step.
  if (!Fs::exists(path)) {
    lucent::warn("boot", "{} missing — extracting from disc", path);
    if (!disc_extract_file(&game->disc, "\\MAIN.EXE", path)) {
      lucent::error(
          "boot",
          "extraction failed: provide a disc (PSXPORT_TOMBA2_DISC, .env, or a *.chd in the working directory) or "
          "run ./run.sh");
      return 1;
    }
  }
  // The product has one execution policy: owned waits and I/O finish synchronously.
  c->game->gpu_vk.tritest(); // PSXPORT_VK_TRITEST=1: GPU triangle-rasterizer self-test, then exit
  watchdog_init();           // PSXPORT_WATCHDOG=<sec>: abort+backtrace if a frame stalls
  load_exe(path, c);
  void games_tomba2_init(void);
  void card_overrides_init(Game *);
  void threads_init(Core *);
  void threads_register_overrides(void);
  gte_init();                        // GTE (COP2) coprocessor, lifted from Beetle
  mdec_init();                       // MDEC video decoder (FMV), lifted from Beetle
  spu_init();                        // SPU audio core, lifted from Beetle
  game->spu_audio.init();            // SDL audio output sink (PSXPORT_NOAUDIO to disable)
  game->gpu.gpu_native_init();       // native GPU renderer (parses the game's GP0 stream)
  game->cd.overridesInit();          // native CD: drive-ready + by-LBA read (S3)
  games_tomba2_init();               // Tomba2 per-game overrides (vblank pacing)
  game->platform_hle.initBuiltins(); // HW sync/wait stalls -> native non-stall (VSync/CdSync/MDEC)
  c->game->pad.overridesInit();      // native controller input (per-VBlank pad read override)
  card_overrides_init(game);         // native memory card (synchronous file-backed libcard I/O)
  threads_init(c);                   // native BIOS threads (ucontext); main = slot 0
  threads_register_overrides();
  c->r[4] = 1;
  c->r[5] = 0; // a0=argc-ish, a1=argv (BIOS sets these; minimal)

  // Bind native overrides only after the executable has established its active image identity.
  c->runtime->registerOverrides(*game); // ALL override clusters — game/core/register_overrides.cpp
  game->stub.run(path);                 // stub draws SCEA, then hands off to native MAIN boot
  c->lightrecExecutor().reportFallbackTelemetry("tomba2-exit");
  lucent::info("boot", "boot stub returned");
  return 0;
}
