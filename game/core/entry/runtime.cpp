#include "entry/runtime.h"

#include "cd/engine_cd.h"
#include "core.h"
#include "crt0_boot.h"
#include "debug/dev_args.h"
#include "debug/repl_commands.h"
#include "engine/engine.h"
#include "entry/main_handoff.h"
#include "entry/session.h"
#include "frame/frame_driver.h"
#include "game.h"
#include "guest_call.h"
#include "lit_model_emitter.h"
#include "overlay_ground_gt3gt4.h"
#include "overlay_gt3gt4.h"
#include "overrides/native_override_catalog.h"
#include "overrides/register_overrides.h"
#include "platform_hle.h"
#include "rain_streaks.h"
#include "render/billboard_state.h"
#include "render/guest_ordering_table.h"
#include "render/packed_mesh.h"
#include "render/screen_fade.h"
#include "resident_natives.h"
#include "sched/scheduler.h"
#include "sched/task_slot_layout.h"
#include "sway_model_emitter.h"
#include "tile_grid_layer.h"
#include "ui/font.h"
#include "ui/ui_sprite.h"
#include "unlit_model_emitter.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <lucent/log.h>
#include <string>

namespace tomba {

namespace {

constexpr std::uint32_t kGameMain = 0x80050B08u;
constexpr GuestAddressRange kResidentText{0x00010000u, 0x000BE800u};
constexpr std::uint32_t kChangeThread = 0x80080880u;
constexpr std::uint32_t kCdReady = 0x8008A96Cu;
constexpr std::uint32_t kCdDataSync = 0x8008B4B8u;
constexpr std::uint32_t kCdInitHandshake = 0x8008B2D8u; // waits on a controller-reset IRQ
constexpr std::uint32_t kDecDctInSync = 0x8009CAECu;
constexpr std::uint32_t kDecDctOutSync = 0x8009CB80u;
constexpr int kDisplayLines = 224; // the letterbox bars sit flush to row 223; GP1(07) is never written

constexpr psx::game::Crt0Image kCrt0{
    .bss = {0x800BE0D8u, 0x80106228u},
    .stackTopWordAddress = 0x800A3F88u,
    .stackReserveWordAddress = 0x800A3F8Cu,
    .heapBase = 0x80106228u,
    .heapSizeStoreAddress = 0x800ABEF8u,
    .heapBaseStoreAddress = 0x800ABEF4u,
    .globalPointer = 0x800BE0D4u,
    .libcInitEntry = 0x80089860u,
    .crt0Entry = 0x800896E0u,
    .stackBias = {.declared = true, .bytes = -8},
};

constexpr psx::gpu::GuestPacketPoolWindows kPacketPool{
    .representation = psx::gpu::GuestPacketPoolWindows::Representation::FixedBaseStride,
    .base = tomba::render::PacketPool::kBase,
    .stride = tomba::render::PacketPool::kStride,
};

constexpr psx::game::HostIdentity kHostIdentity{.userDataName = "tomba2"};

void changeThread(Core *core) {
  sched::taskScheduler(*core).yield(*core);
}

// CdReady(mode, result): the drive is always idle and its result buffer empty.
void cdReady(Core *core) {
  const std::uint32_t result = core->r[5];
  for (std::uint32_t index = 0; result != 0 && index < 8; ++index) {
    core->mem_w8(result + index, 0);
  }
  core->r[2] = 0;
}

void callGuest(Core &core, std::uint32_t address) {
  psx::cpu::dispatchGuestToReturn0(core, address, psx::cpu::ExecutionBudget::currentTurn(core), "Runtime::bootInit");
}

void callGuest(Core &core, std::uint32_t address, std::uint32_t a0) {
  psx::cpu::callGuestNow(core, "Runtime::bootInit", address, a0);
}

} // namespace

Runtime::Runtime()
    : vsync_({.entry = 0x80085900u}), libcd_({.command = 0x8008AC34u, .sync = 0x8008A6ECu}, {}, 0),
      libgpu_(kStockLibgpu), libgte_({.setGeomOffset = 0x800846D0u, .setGeomScreen = 0x800846F0u}),
      pad_({.slot0Buffer = 0x800BF4F8u, .slot1Buffer = 0x800BF51Au, .slotPointerTable = 0x0000AEC8u}) {}

void Runtime::startProgram(Core &core) {
  psx::game::Crt0Boot{kCrt0}.run(core);
  bootInit(core);
}

void Runtime::registerOverrides(Game &game) {
  native::NativeOverrideCatalog &natives = sessionOf(&game.core).nativeOverrides;
  // The host loaded the boot stub; its only job is the hand-off to MAIN.EXE.
  loadMainExecutable(game);
  vsync_.bind(game);
  libcd_.bind(game);
  libgpu_.bind(game);
  libgte_.bind(game);
  game.platform_hle.bindStock(kDecDctInSync, PlatformHle::completeImmediately);
  game.platform_hle.bindStock(kDecDctOutSync, PlatformHle::completeImmediately);
  game.platform_hle.bindStock(kCdDataSync, PlatformHle::completeImmediately);
  game.platform_hle.bindStock(kCdInitHandshake, PlatformHle::completeImmediately);
  game.platform_hle.bindStock(kCdReady, cdReady);
  installResidentNatives(natives);
  game.memcard.install();
  register_engine_overrides(game, natives);
  EngineCd::declareOverrides(natives);
  natives.declare(kChangeThread, "Scheduler::yield", changeThread);
  bindLoadedResident(game.core);
  psx::present::StateProducerTable &renders = sessionOf(&game.core).stateProducers;
  tomba::render::UnlitModelEmitter::registerStateRenders(renders, game.core);
  OverlayGt3Gt4::registerStateRenders(renders, game.core);
  tomba::render::SwayModelEmitter::registerStateRenders(renders, game.core);
  tomba::render::LitModelEmitter::registerStateRenders(renders, game.core);
  OverlayGroundGt3Gt4::registerStateRenders(renders, game.core);
  TileGridLayer::registerStateRenders(renders, game.core);
  RainStreaks::registerStateRenders(renders);
  Font::registerStateRenders(renders);
  ScreenFade::registerStateRenders(renders);
  UiSprite::registerStateRenders(renders);
  tomba::render::BillboardState::registerStateRenders(renders, game.core);
  tomba::render::PackedMesh::registerStateRenders(renders, game.core);
}

// Only immediately after the resident load; an overlay supplies its own image declarations.
void Runtime::bindLoadedResident(Core &core) {
  const auto resident = core.currentImageIdentity(kGameMain);
  if (!resident) {
    lucent::error("tomba-native", "resident load did not establish the MAIN.EXE image");
    std::abort();
  }
  sessionOf(&core).nativeOverrides.bindResident(core, *resident, kResidentText);
}

std::unique_ptr<psx::game::TitleSessionState> Runtime::createSession(Game &game) {
  return std::make_unique<Session>(game.core);
}

std::unique_ptr<psx::game::FrameDriver> Runtime::createFrameDriver(Game &game) {
  return std::make_unique<FrameDriver>(game);
}

void Runtime::deliverPad(Core &core, const psx::input::PadDelivery &delivery) {
  pad_.deliver(core, delivery);
}

std::optional<psx::gpu::PacketPoolExtent> Runtime::packetPool(Core &core) const {
  return kPacketPool.extent(core);
}

bool Runtime::interpolates() const {
  return true;
}

const psx::present::StateProducer *Runtime::stateProducer(Core &core, std::uint32_t producer) const {
  return sessionOf(&core).stateProducers.find(producer);
}

bool Runtime::sealedFrameIsCut(Core &core) const {
  return eng(&core).frameCut().isCut();
}

int Runtime::displayHeight() const {
  return kDisplayLines;
}

const GuestWidescreenProjection *Runtime::guestWidescreenProjection() const {
  return &aspectPolicy_;
}

void Runtime::bootInit(Core &core) {
  Core *c = &core;
  lucent::info("native_boot", "FUN_80050b08: running init prefix");

  // Retail order: task0Bootstrap depends on the scheduler table and current-task writes between them.
  callGuest(core, 0x80089788u);
  callGuest(core, 0x80085B20u);
  // The guest CdInit busy-waits on a controller-reset IRQ; every disc read here is synchronous.
  sessionOf(c).cd.hleInit();
  callGuest(core, 0x80080BF0u, 3);
  callGuest(core, 0x80080D64u, 0);
  callGuest(core, 0x80080ED4u, 1);
  callGuest(core, 0x800865F0u, 0);
  eng(c).initFrameState(); // FUN_80050A0C
  eng(c).initDisplay();    // FUN_800509B4; projection H for camera init
  eng(c).initCamera();     // FUN_80050A80
  callGuest(core, 0x80096A70u);
  callGuest(core, 0x80099310u, 0x1010);
  callGuest(core, 0x800991B0u, 0x20000);
  callGuest(core, 0x800993A0u, 1);
  // CdControlB(Setmode) and its VSync settle waits are absent: reads are by LBA and guest VSync traps.
  eng(c).font.init(); // FUN_80075130
  callGuest(core, 0x8009C620u, 0);
  callGuest(core, 0x8001CC00u);
  eng(c).initSubsystems();                                                        // FUN_800520E0
  callGuest(core, 0x80051E00u);                                                   // scheduler table at 0x801FE000
  psx::cpu::callGuestNow(core, "Runtime::bootInit", 0x80051F14u, 0, 0x800499E8u); // task 0 at FUN_800499E8
  lucent::info("native_boot", "init prefix complete");

  // FUN_80051E60 would set the current task; task0Bootstrap needs it to name slot 0.
  c->mem_w32(sched::kCurrentTaskPointer, sched::kTaskTableBase);
  eng(c).task0Bootstrap(); // FUN_800499E8
  lucent::info("native_boot",
               "after FUN_800499e8: START.BIN count@0x80106228={} entry-word@0x8010649c=0x{:08X} "
               "(expect 0x27BDFE38); task0 state={} entry=0x{:08X}",
               c->mem_r32(0x80106228u),
               c->mem_r32(sched::kStageStartEntry),
               c->mem_r16(sched::kTaskTableBase + sched::kTaskState),
               c->mem_r32(sched::kTaskTableBase + sched::kTaskEntry));
  FrameDriver::enterLoop(core);
}

bool Runtime::titleCommand(Core &core, const psx::game::TitleCommandLine &input, std::FILE *reply) {
  const auto [command, line] = input;
  auto *driver = dynamic_cast<FrameDriver *>(core.game->frameDriver.get());
  if (driver == nullptr) {
    lucent::error("tomba-native", "{} arrived with no Tomba! 2 frame driver", command);
    std::abort();
  }
  std::string answer;
  if (driver->devControl().handle(core, line, answer)) {
    std::fprintf(reply, "%s\n", answer.c_str());
    return true;
  }
  if (std::strcmp(command, "tp") == 0) {
    const auto words = DevArgs::words(line);
    int32_t x = 0;
    int32_t y = 0;
    int32_t z = 0;
    if (words.size() == 4 && DevArgs::integer(words[1], x) && DevArgs::integer(words[2], y) &&
        DevArgs::integer(words[3], z)) {
      eng(&core).camTeleport({.x = x, .y = y, .z = z});
      std::fprintf(reply, "tp camera -> (%d,%d,%d)\n", x, y, z);
    } else {
      eng(&core).camTeleportOff();
      std::fprintf(reply, "tp off (camera follows player)\n");
    }
    return true;
  }
  // The other developer commands (bgm, invtest, ...) answer through the log.
  const bool handled = replCommand(core, line);
  if (handled) {
    std::fprintf(reply, "ok: %s\n", command);
  }
  return handled;
}

psx::game::TitleAudio *Runtime::audio(Core &core) const {
  return &sessionOf(&core).audio;
}

psx::game::TitleDevWarp *Runtime::devWarp(Core &core) const {
  return &sessionOf(&core).devWarp;
}

// Tomba's position from the scratchpad camera target, and the stage task 0 runs.
psx::state::NativeStatePort *Runtime::nativeState(Core &core) const {
  return &sessionOf(&core).cdState;
}

std::string Runtime::worldReadout(Core &core) const {
  char text[96];
  std::snprintf(text,
                sizeof text,
                "pos (%d, %d, %d)  stage %s",
                core.mem_r16s(0x1F8000D2u),
                core.mem_r16s(0x1F8000D6u),
                core.mem_r16s(0x1F8000DAu),
                sched::stageName(core.mem_r32(sched::kTaskTableBase + sched::kTaskEntry)));
  return text;
}

const psx::game::HostIdentity *Runtime::hostIdentity() const {
  return &kHostIdentity;
}

} // namespace tomba
