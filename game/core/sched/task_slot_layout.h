// task_slot_layout.h — Tomba! 2's cooperative task table (FUN_80051E60's slots) and stage entries.
#pragma once

#include <cstdint>

namespace tomba::sched {

inline constexpr std::uint32_t kTaskTableBase = 0x801FE000u;
inline constexpr std::uint32_t kTaskSlotStride = 0x70u;
inline constexpr int kTaskSlotCount = 3;
// Scratchpad word holding the running task's slot base (DAT_1F800138).
inline constexpr std::uint32_t kCurrentTaskPointer = 0x1F800138u;

// Slot fields.
inline constexpr std::uint32_t kTaskState = 0x00u;     // u16, see TaskState
inline constexpr std::uint32_t kTaskCountdown = 0x02u; // u16 sleep countdown, FUN_800506D0
inline constexpr std::uint32_t kTaskHandle = 0x04u;    // BIOS OpenTh handle
inline constexpr std::uint32_t kTaskStack = 0x08u;     // the task's own guest stack top
inline constexpr std::uint32_t kTaskEntry = 0x0Cu;     // stage body PC
inline constexpr std::uint32_t kTaskGp = 0x10u;
inline constexpr std::uint32_t kStateMachine = 0x48u;  // task 0's stage state word
inline constexpr std::uint32_t kFieldSubMode = 0x4Au;  // 0 intro machine, 1 field
inline constexpr std::uint32_t kFieldRunState = 0x4Eu; // the field machine's own state
inline constexpr std::uint32_t kSyncWaitStamp = 0x56u; // FUN_80044BD4's RNG stamp
inline constexpr std::uint32_t kTaskRunFlag = 0x6Cu;
inline constexpr std::uint32_t kTaskSwitchFlag = 0x6Fu;

enum class TaskState : std::uint8_t {
  Ended = 0,
  Yielded = 1, // FUN_800506D0 re-arms it to Runnable when its countdown reaches zero
  Runnable = 2,
  Restart = 3, // FUN_80052078 rewrote the entry
  Running = 4,
};

// Stage bodies task 0 runs, and the loader bodies it spawns.
inline constexpr std::uint32_t kStageStartEntry = 0x8010649Cu; // START.BIN stage 0
inline constexpr std::uint32_t kStageDemoEntry = 0x801062E4u;  // DEMO / front-end menu
inline constexpr std::uint32_t kStageGameEntry = 0x8010637Cu;  // GAME
inline constexpr std::uint32_t kGameLoopTop = 0x801063F4u;     // GAME's cooperative loop head
inline constexpr std::uint32_t kSopAreaLoadEntry = 0x80109164u;

constexpr std::uint32_t taskSlotBase(int slot) {
  return kTaskTableBase + static_cast<std::uint32_t>(slot) * kTaskSlotStride;
}

// FUN_80044BD4's synchronous spawn-and-wait words and call-site return addresses.
inline constexpr std::uint32_t kSyncWaitDoneFlag = 0x1F80019Bu;
inline constexpr std::uint32_t kSyncWaitParam2 = 0x801FE0DEu;
inline constexpr std::uint32_t kSyncWaitParam3 = 0x801FE0DDu;
inline constexpr std::uint32_t kScratchpadBase = 0x1F800000u;
inline constexpr std::uint32_t kSyncWaitForceCloseRa = 0x80044C2Cu;
inline constexpr std::uint32_t kSyncWaitSpawnRa = 0x80044C50u;
inline constexpr std::uint32_t kSyncWaitFinishRa = 0x80044C64u;

// The stage a task-0 entry names, or "?".
constexpr const char *stageName(std::uint32_t entry) {
  switch (entry) {
  case kStageStartEntry:
    return "START";
  case kStageDemoEntry:
    return "DEMO";
  case kStageGameEntry:
    return "GAME";
  default:
    return "?";
  }
}

} // namespace tomba::sched
