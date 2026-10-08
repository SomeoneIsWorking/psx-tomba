#include "native_boot.h"

#include "cd_native_startup.h"
#include "core.h"
#include "guest_call.h"

#include <cstdint>
#include <cstdlib>
#include <lucent/log.h>

namespace tomba1 {
namespace {

constexpr std::uint32_t kBssBegin = 0x8009AFB0u;
constexpr std::uint32_t kBssEnd = 0x800A3348u;
constexpr std::uint32_t kStackTopWord = 0x80076E38u;
constexpr std::uint32_t kStackReserveWord = 0x80076E3Cu;
constexpr std::uint32_t kHeapBase = 0x800A3348u;
constexpr std::uint32_t kHeapSizeStore = 0x800975C0u;
constexpr std::uint32_t kHeapBaseStore = 0x800975BCu;
constexpr std::uint32_t kGlobalPointer = 0x80097FA8u;

constexpr std::uint32_t kLibcInitHeap = 0x8006B70Cu;
constexpr std::uint32_t kCxxInitGuard = 0x8006B634u;
constexpr std::uint32_t kSetDisplayMask = 0x8005EAB8u;
constexpr std::uint32_t kStopCallback = 0x800689D4u;
constexpr std::uint32_t kResetCallback = 0x80067E24u;
constexpr std::uint32_t kInitializeGameState = 0x80016A18u;
constexpr std::uint32_t kResetGraph = 0x8005E694u;
constexpr std::uint32_t kSetGraphDebug = 0x8005E92Cu;
constexpr std::uint32_t kInitializeGte = 0x8006329Cu;
constexpr std::uint32_t kInitializePad = 0x8005CFE8u;
constexpr std::uint32_t kInitializeDisplay = 0x80016AF4u;
constexpr std::uint32_t kSetFrameCadence = 0x80016A00u;
constexpr std::uint32_t kInitializeSound = 0x800211A4u;
constexpr std::uint32_t kInitializeRect = 0x8005DD6Cu;
constexpr std::uint32_t kInitializeMenu = 0x80019020u;
constexpr std::uint32_t kInitializeInput = 0x80028728u;
constexpr std::uint32_t kInitializeTasks = 0x80016FD8u;
constexpr std::uint32_t kCreateTask = 0x800170F8u;
constexpr std::uint32_t kTaskEntry = 0x800191E0u;
constexpr std::uint32_t kEnterCritical = 0x8005B41Cu;
constexpr std::uint32_t kOpenEvent = 0x8005B3ACu;
constexpr std::uint32_t kExitCritical = 0x8005B42Cu;
constexpr std::uint32_t kEnableEvent = 0x8005B3DCu;

constexpr std::uint32_t kVblankEventClass = 0xF2000003u;
constexpr std::uint32_t kVblankEventSpec = 2u;
constexpr std::uint32_t kInterruptEventMode = 0x1000u;
constexpr std::uint32_t kVblankCallback = 0x80017374u;
constexpr std::uint32_t kVblankHandle = 0x1F8001D8u;

void initializeCrt0State(Core &core) {
  for (std::uint32_t address = kBssBegin; address < kBssEnd; address += sizeof(std::uint32_t)) {
    core.mem_w32(address, 0u);
  }

  const std::uint32_t stackTop = (core.mem_r32(kStackTopWord) - 8u) | 0x80000000u;
  const std::uint32_t stackReserve = core.mem_r32(kStackReserveWord);
  if (stackTop <= kHeapBase + stackReserve) {
    lucent::error("tomba1-boot",
                  "measured crt0 stack/heap relation is invalid: top=0x{:08X} reserve=0x{:08X}",
                  stackTop,
                  stackReserve);
    std::abort();
  }

  const std::uint32_t heapSize = stackTop - stackReserve - kHeapBase;
  core.r[28] = kGlobalPointer;
  core.r[29] = stackTop;
  core.mem_w32(kHeapSizeStore, heapSize);
  core.mem_w32(kHeapBaseStore, kHeapBase);
  psx::cpu::dispatchGuestToReturn2(
      core, kLibcInitHeap, kHeapBase + 4u, heapSize, psx::cpu::ExecutionBudget::currentTurn(core), __func__);
}

} // namespace

void runNativeBootPrefix(Core &core) {
  initializeCrt0State(core);

  // Keep the retail 0x800163B0 initialization order, but stop before its non-returning scheduler
  // loop. The 48-byte main activation remains live for process lifetime, as it does on the console.
  core.r[29] -= 48u;
  core.mem_w32(core.r[29] + 44u, core.r[31]);
  core.mem_w32(core.r[29] + 40u, core.r[18]);
  core.mem_w32(core.r[29] + 36u, core.r[17]);
  core.mem_w32(core.r[29] + 32u, core.r[16]);

  psx::cpu::dispatchGuestToReturn0(core, kCxxInitGuard, psx::cpu::ExecutionBudget::currentTurn(core), __func__);
  psx::cpu::dispatchGuestToReturn1(core, kSetDisplayMask, 0u, psx::cpu::ExecutionBudget::currentTurn(core), __func__);
  psx::cpu::dispatchGuestToReturn1(core, kStopCallback, 0u, psx::cpu::ExecutionBudget::currentTurn(core), __func__);
  psx::cpu::dispatchGuestToReturn0(core, kResetCallback, psx::cpu::ExecutionBudget::currentTurn(core), __func__);
  psx::cpu::dispatchGuestToReturn0(core, kInitializeGameState, psx::cpu::ExecutionBudget::currentTurn(core), __func__);
  psx::cpu::dispatchGuestToReturn1(core, kResetGraph, 0u, psx::cpu::ExecutionBudget::currentTurn(core), __func__);
  psx::cpu::dispatchGuestToReturn1(core, kSetGraphDebug, 0u, psx::cpu::ExecutionBudget::currentTurn(core), __func__);
  psx::cpu::dispatchGuestToReturn0(core, kInitializeGte, psx::cpu::ExecutionBudget::currentTurn(core), __func__);

  const std::uint32_t cdResult = core.r[29] + 24u;
  initializeSynchronousCd(core, cdResult);

  psx::cpu::dispatchGuestToReturn1(core, kInitializePad, 0u, psx::cpu::ExecutionBudget::currentTurn(core), __func__);
  core.mem_w32(0x1F8002A0u, 0u);
  core.mem_w32(0x1F80029Cu, 0u);
  psx::cpu::dispatchGuestToReturn0(core, kInitializeDisplay, psx::cpu::ExecutionBudget::currentTurn(core), __func__);
  psx::cpu::dispatchGuestToReturn2(
      core, kSetFrameCadence, 1u, 1u, psx::cpu::ExecutionBudget::currentTurn(core), __func__);
  psx::cpu::dispatchGuestToReturn0(core, kInitializeSound, psx::cpu::ExecutionBudget::currentTurn(core), __func__);

  core.mem_w32(core.r[29] + 16u, 240u);
  psx::cpu::dispatchGuestToReturn4(
      core, kInitializeRect, 0x8009AFE8u, 0u, 0u, 512u, psx::cpu::ExecutionBudget::currentTurn(core), __func__);
  psx::cpu::dispatchGuestToReturn0(core, kInitializeMenu, psx::cpu::ExecutionBudget::currentTurn(core), __func__);
  psx::cpu::dispatchGuestToReturn0(core, kInitializeInput, psx::cpu::ExecutionBudget::currentTurn(core), __func__);
  psx::cpu::dispatchGuestToReturn0(core, kInitializeTasks, psx::cpu::ExecutionBudget::currentTurn(core), __func__);
  psx::cpu::dispatchGuestToReturn2(
      core, kCreateTask, 0u, kTaskEntry, psx::cpu::ExecutionBudget::currentTurn(core), __func__);

  psx::cpu::dispatchGuestToReturn0(core, kEnterCritical, psx::cpu::ExecutionBudget::currentTurn(core), __func__);
  psx::cpu::dispatchGuestToReturn4(core,
                                   kOpenEvent,
                                   kVblankEventClass,
                                   kVblankEventSpec,
                                   kInterruptEventMode,
                                   kVblankCallback,
                                   psx::cpu::ExecutionBudget::currentTurn(core),
                                   __func__);
  core.mem_w32(kVblankHandle, core.r[2]);
  psx::cpu::dispatchGuestToReturn0(core, kExitCritical, psx::cpu::ExecutionBudget::currentTurn(core), __func__);
  psx::cpu::dispatchGuestToReturn1(
      core, kEnableEvent, core.mem_r32(kVblankHandle), psx::cpu::ExecutionBudget::currentTurn(core), __func__);
  psx::cpu::dispatchGuestToReturn1(core, kSetDisplayMask, 1u, psx::cpu::ExecutionBudget::currentTurn(core), __func__);

  lucent::info("tomba1-boot",
               "retail initialization prefix complete; task entry=0x{:08X} vblank-handle=0x{:08X}",
               kTaskEntry,
               core.mem_r32(kVblankHandle));
}

} // namespace tomba1
