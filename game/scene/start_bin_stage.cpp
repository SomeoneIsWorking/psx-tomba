// start_bin_stage.cpp — see header.
#include "start_bin_stage.h"
#include "cd/libcd_dir_cache.h" // class LibcdDirCache — native_sync writer of libcd's guest dir cache
#include "cd/libcd_native.h"    // class LibcdNative — pc_faithful guest libcd chain
#include "core.h"
#include "core/assets/asset.h"
#include "game.h"
#include "guest_call.h"
#include "sched/task_slot_layout.h"
#include "sched/tomba_scheduler.h"
#include <lucent/log.h>

namespace tomba {

namespace {
// A START.BIN CdSearchFile loop: a table of guest filename pointers resolved into a parallel
// {LBA,size} destination table.
struct FileTable {
  uint32_t names, dest, count;
  uint32_t raSearch, raPosToInt, raPrintf; // guest call-site ra constants (pc_faithful identity)
};
constexpr FileTable kFileTables[] = {
    {0x80106808u, 0x800BE118u, 25, 0x8010651Cu, 0x80106540u, 0x80106530u}, // \BIN\OPN/CRD/SOP/A00..A0L
    {0x8010686Cu, 0x800BE1E0u, 3, 0x80106594u, 0x801065B8u, 0x801065A8u},  // \BIN\START/DEMO/GAME.BIN
    {0x801067F4u, 0x800BE0F0u, 5, 0x8010660Cu, 0x80106630u, 0x80106620u},  // \CD\TOMBA2.IDX/IMG/DAT/SND + SWDATA.BIN
};
// An XA singleton: one guest filename resolved to one scratchpad LBA word.
struct XaFile {
  uint32_t name, dest;
  uint32_t raSearch, raPosToInt, raPrintf;
};
constexpr XaFile kXaFiles[] = {
    {0x8010646Cu, 0x1F80021Cu, 0x80106670u, 0x80106694u, 0x80106684u}, // \CD\VOICE.XA
    {0x8010647Cu, 0x1F800220u, 0x801066B4u, 0x801066D8u, 0x801066C8u}, // \CD\DEMO.XA
    {0x8010648Cu, 0x1F800224u, 0x801066F8u, 0x8010671Cu, 0x8010670Cu}, // \CD\BGM.XA
};
constexpr uint32_t kNotFoundFmt = 0x80106454u; // "Not found file name %s"
constexpr uint32_t kGuestPrintf = 0x8009A730u;

} // namespace

bool StartBinStage::resolveViaIso9660(uint32_t name_ptr, uint32_t *lba, uint32_t *size) {
  char name[80];
  core_.readCString(name_ptr, name, sizeof name);
  if (disc_find_file(&core_.game->disc, name, lba, size)) {
    return true;
  }
  lucent::info("start.bin", "not found: {}", name);
  return false;
}

void StartBinStage::resolveFileTablesNative() {
  for (const auto &table : kFileTables) {
    for (uint32_t i = 0; i < table.count; i++) {
      uint32_t lba = 0;
      uint32_t size = 0;
      resolveViaIso9660(core_.mem_r32(table.names + i * 4), &lba, &size);
      core_.mem_w32(table.dest + i * 8, lba);
      core_.mem_w32(table.dest + i * 8 + 4, size);
    }
  }
  for (const auto &xa : kXaFiles) {
    uint32_t lba = 0;
    uint32_t size = 0;
    resolveViaIso9660(xa.name, &lba, &size);
    core_.mem_w32(xa.dest, lba);
  }
}

// native_sync only — the pc_faithful body splits these writes across their real guest call sites
// (task-1 spawn for the preload, FUN_80044BD4 tail for sm[0x48]:=1, FUN_80044BD4 body for the RNG
// stamp).
void StartBinStage::advanceWithBootPreload() {
  const uint32_t task = core_.mem_r32(tomba::sched::kCurrentTaskPointer);
  core_.mem_w16(task + 0x4a, 0);
  asset_.preloadTexgroup({.mode = 0, .set = 0});
  tomba::sched::taskScheduler(core_).completeSyncWait(task, 0);
  core_.mem_w16(task + 0x48, 1);
}

// Collapsed native shortcut. Native VRAM upload (bypasses libgs LoadImage), native ISO9660 file
// lookup (bypasses libcd — its dir/file cache is written from the same ISO9660 sectors so any
// post-boot guest dispatch into CdSearchFile short-circuits its new-media branch), inline
// preloads (bypass the task-1 spawn), task-1 slot closed with state=0 as if it ran and completed
// inline. No scheduler-only frames are manufactured for already-synchronous host work.
void StartBinStage::runNative() {
  core_.mem_w16(0x1F800008u + 0, 944);
  core_.mem_w16(0x1F800008u + 2, 256);
  core_.mem_w16(0x1F800008u + 4, 16);
  core_.mem_w16(0x1F800008u + 6, 1);
  asset_.uploadImage({.desc = 0x1F800008u, .src = kLoadImageSrc});

  LibcdDirCache libcd(core_);
  libcd.newMedia();
  libcd.stampMediaCookie();
  libcd.cacheFile(2);
  libcd.cacheFile(3);

  resolveFileTablesNative();

  advanceWithBootPreload();
  const uint32_t task = core_.mem_r32(tomba::sched::kCurrentTaskPointer);
  core_.mem_w16(task + 0x48, 2);
  asset_.preloadStage1();
  tomba::sched::taskScheduler(core_).completeSyncWait(task, 0);
  core_.mem_w16(task + 0x48, 3);
  lucent::info("start.bin", "file table and boot preloads completed synchronously");
}

} // namespace tomba
