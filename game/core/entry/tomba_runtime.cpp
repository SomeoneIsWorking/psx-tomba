#include "entry/tomba_runtime.h"

#include "core.h"
#include "engine/engine.h"
#include "entry/game_ctx.h"
#include "entry/main_handoff.h"
#include "frame/frame_driver.h"
#include "game.h"
#include "game_tomba2.h"
#include "guest_call.h"
#include "lit_model_emitter.h"
#include "memcard.h"
#include "overlay_ground_gt3gt4.h"
#include "overlay_gt3gt4.h"
#include "overrides/native_override_catalog.h"
#include "overrides/register_overrides.h"
#include "rain_streaks.h"
#include "sway_model_emitter.h"
#include "tile_grid_layer.h"
#include "title_facts.h"
#include "ui/font.h"
#include "unlit_model_emitter.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <lucent/log.h>
#include <string>

namespace tomba {

TombaRuntime::TombaRuntime() : LegacyGameRuntimeAdapter(title::measuredConfig(), title::hooks()) {}

void *TombaRuntime::createContext(Core &core) {
  return createTombaContext(core);
}

void TombaRuntime::destroyContext(void *context) {
  destroyTombaContext(context);
}

void TombaRuntime::registerOverrides(Game &game) {
  // The host loaded the boot stub; its only job is the hand-off to MAIN.EXE, which the declarations
  // below bind to.
  loadMainExecutable(game);
  games_tomba2_init();
  card_overrides_init(&game);
  register_engine_overrides(game);
  bindLoadedResident(game.core);
  tomba2::render::UnlitModelEmitter::registerStateRenders(game.core);
  OverlayGt3Gt4::registerStateRenders(game.core);
  tomba2::render::SwayModelEmitter::registerStateRenders(game.core);
  tomba2::render::LitModelEmitter::registerStateRenders(game.core);
  OverlayGroundGt3Gt4::registerStateRenders(game.core);
  TileGridLayer::registerStateRenders(game.core);
  RainStreaks::registerStateRenders(game.core);
  Font::registerStateRenders(game.core);
}

void TombaRuntime::bindLoadedResident(Core &core) {
  // Called only immediately after the resident load.
  // Overlay activation must supply its own image-specific declarations instead.
  const auto *program = guestProgramImage();
  const auto resident = program ? core.currentImageIdentity(program->gameMainEntry) : std::nullopt;
  if (!program || !program->residentText.valid() || !resident) {
    lucent::error("tomba-native", "resident load did not establish the declared MAIN image");
    std::abort();
  }
  native::bindResident(core, *resident, program->residentText);
}

std::unique_ptr<FrameDriver> TombaRuntime::createFrameDriver(Game &game) {
  return std::make_unique<TombaFrameDriver>(game);
}

RenderCapabilities TombaRuntime::renderCapabilities() const {
  // The picture is the guest's GP0 output, replayed from the frame record; there is no native renderer.
  return RenderCapabilities{
      .defaultPath = RenderPath::Record,
      .nativeRenderPath = false,
      .temporalInterpolation = true,
  };
}

bool TombaRuntime::guestVramIsPicture(const Game &) const {
  return true;
}

bool TombaRuntime::sealedFrameIsCut(Core &core) const {
  return eng(&core).frameCut().isCut();
}

void TombaRuntime::bootInit(Core &core) {
  Core *c = &core;
  lucent::info("native_boot", "FUN_80050b08 override: running init prefix");

  // FUN_80050b08's init prefix, without its scheduler loop. The guest calls and native engine
  // owners are deliberately kept in retail order because task0Bootstrap depends on the scheduler
  // table and current-task writes between them.
  psx::cpu::dispatchGuestToReturn0(*c, 0x80089788, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  psx::cpu::dispatchGuestToReturn0(*c, 0x80085b20, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  // The guest CdInit busy-waits on a controller-reset IRQ that this native host does not model.
  // Every disc operation is synchronous and native, so initialize that owner directly.
  c->game->cd.hleInit();
  psx::cpu::dispatchGuestToReturn1(*c, 0x80080bf0, 3, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  psx::cpu::dispatchGuestToReturn1(*c, 0x80080d64, 0, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  psx::cpu::dispatchGuestToReturn1(*c, 0x80080ed4, 1, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  psx::cpu::dispatchGuestToReturn1(*c, 0x800865f0, 0, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  eng(c).initFrameState(); // was FUN_80050a0c
  eng(c).initDisplay();    // was FUN_800509b4; establishes projection H for camera init
  eng(c).initCamera();     // was FUN_80050a80
  psx::cpu::dispatchGuestToReturn0(*c, 0x80096a70, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  psx::cpu::dispatchGuestToReturn1(*c, 0x80099310, 0x1010, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  psx::cpu::dispatchGuestToReturn1(*c, 0x800991b0, 0x20000, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  psx::cpu::dispatchGuestToReturn1(*c, 0x800993a0, 1, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  // The original CdControlB(Setmode) is intentionally absent: reads are native by LBA and there is
  // no emulated drive mode. Its following VSync settle waits are absent too; the host frame loop is
  // the sole timing owner and guest VSync is a trap.
  eng(c).font.init(); // was FUN_80075130
  psx::cpu::dispatchGuestToReturn1(*c, 0x8009c620, 0, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  psx::cpu::dispatchGuestToReturn0(*c, 0x8001cc00, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
  eng(c).initSubsystems(); // was FUN_800520e0
  psx::cpu::dispatchGuestToReturn0(*c,
                                   0x80051e00,
                                   psx::cpu::ExecutionBudget::currentTurn(*c),
                                   __func__); // scheduler table: task objects at 0x801fe000
  psx::cpu::dispatchGuestToReturn2(*c,
                                   0x80051f14,
                                   0,
                                   0x800499e8,
                                   psx::cpu::ExecutionBudget::currentTurn(*c),
                                   __func__); // register task 0 at FUN_800499e8
  lucent::info("native_boot", "init prefix complete");

  // FUN_800499E8 resolves START.BIN and FUN_80052078(0) installs stage 0. The guest scheduler would
  // normally establish DAT_1f800138 in FUN_80051e60; task0Bootstrap needs it to name slot 0.
  c->mem_w32(0x1f800138, 0x801fe000);
  eng(c).task0Bootstrap(); // was FUN_800499e8; CD subtree is owned top-down
  lucent::info("native_boot",
               "after FUN_800499e8: START.BIN count@0x80106228={} "
               "entry-word@0x8010649c=0x{:08X} (expect 0x27BDFE38); task0 state={} "
               "entry=0x{:08X}",
               c->mem_r32(0x80106228),
               c->mem_r32(0x8010649c),
               c->mem_r16(0x801fe000),
               c->mem_r32(0x801fe00c));
  TombaFrameDriver::enterLoop(core);
}

const GuestWidescreenProjection *TombaRuntime::guestWidescreenProjection() const {
  return &aspectPolicy_;
}

bool TombaRuntime::controlCommand(Core &core, const char *cmd, const char *line, FILE *out) {
  auto *driver = dynamic_cast<TombaFrameDriver *>(core.game->frameDriver.get());
  if (driver == nullptr) {
    lucent::error("tomba-native", "{} arrived with no Tomba! 2 frame driver", cmd);
    std::abort();
  }
  std::string reply;
  if (driver->devControl().handle(core, cmd, line, reply)) {
    std::fprintf(out, "%s\n", reply.c_str());
    return true;
  }
  // The title's other developer commands (bgm, invtest, ...) answer through the log.
  const bool handled = replCommand(core, cmd, line);
  if (handled) {
    std::fprintf(out, "ok: %s\n", cmd);
  }
  return handled;
}

} // namespace tomba
