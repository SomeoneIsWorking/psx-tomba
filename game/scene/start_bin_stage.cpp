// start_bin_stage.cpp — see header.
#include "start_bin_stage.h"
#include "cd/libcd_dir_cache.h" // class LibcdDirCache — native_sync writer of libcd's guest dir cache
#include "cd/libcd_native.h"    // class LibcdNative — pc_faithful guest libcd chain
#include "cfg.h"
#include "core.h"
#include "core/assets/asset.h"
#include "disc.h" // disc_find_file — native ISO9660 resolver
#include "game.h" // Game::disc, Game::pcSched
#include "guest_call.h"
#include "scheduler.h" // CUR_TASK

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
  cfg_logi("start.bin", "not found: %s", name);
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
  const uint32_t task = core_.mem_r32(CUR_TASK);
  core_.mem_w16(task + 0x4a, 0);
  asset_.preloadTexgroup(0, 0);
  core_.game->pcSched.completeSyncWait(task, 0);
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
  asset_.uploadImage(0x1F800008u, kLoadImageSrc);

  LibcdDirCache libcd(core_);
  libcd.newMedia();
  libcd.stampMediaCookie();
  libcd.cacheFile(2);
  libcd.cacheFile(3);

  resolveFileTablesNative();

  advanceWithBootPreload();
  const uint32_t task = core_.mem_r32(CUR_TASK);
  core_.mem_w16(task + 0x48, 2);
  asset_.preloadStage1();
  core_.game->pcSched.completeSyncWait(task, 0);
  core_.mem_w16(task + 0x48, 3);
  cfg_logi("start.bin", "file table and boot preloads completed synchronously");
}

// The COMPLETE overlay guest 0x8010649C task body. Frame: sp -= 456; LoadImage RECT at sp+400,
// per-iteration CdlFILE records at sp+16+i*24, XA CdlFILE at sp+408; ra/s4..s0 spilled at
// 452..432. NOTE fiber teardown unwinds via longjmp — no destructibles may live across a yield.
void StartBinStage::runFaithful() {
  Core &c = core_;
  psx::frame::PcScheduler &sched = c.game->pcSched;

  // Prologue spills the live frame-loop registers + the 0xDEAD0000 entry ra, same as core B's fiber.
  c.r[29] -= 456;
  const uint32_t S = c.r[29];
  c.mem_w32(S + 452, c.r[31]);
  c.mem_w32(S + 448, c.r[20]);
  c.mem_w32(S + 444, c.r[19]);
  c.mem_w32(S + 440, c.r[18]);
  c.mem_w32(S + 436, c.r[17]);
  c.mem_w32(S + 432, c.r[16]);

  // LoadImage RECT {944,256,16,1} is a frame local at sp+400 (the native shortcut's lives in
  // scratchpad).
  c.mem_w16(S + 400, 944);
  c.mem_w16(S + 402, 256);
  c.mem_w16(S + 404, 16);
  c.mem_w16(S + 406, 1);
  c.r[4] = S + 400;
  c.r[5] = kLoadImageSrc;
  c.r[31] = 0x801064E8u;
  psx::cpu::callGuestNow(c, __func__, 0x80081218u); // libgs LoadImage
  c.r[4] = 0;
  c.r[31] = 0x801064F0u;
  psx::cpu::callGuestNow(c, __func__, 0x80080F6Cu); // DrawSync(0)

  // Three CdSearchFile loops. Loop registers live in the guest s-regs (r16=CdlFILE ptr,
  // r17=name-table ptr, r18=dest ptr, r19=index, r20=i*24) because CdSearchFile's prologue spills
  // them into its frame — compared task-0 stack bytes.
  LibcdNative libcd(&c);
  for (const auto &table : kFileTables) {
    c.r[19] = 0;
    c.r[20] = 0;
    c.r[18] = table.dest;
    c.r[17] = table.names;
    do {
      c.r[16] = S + 16 + c.r[20]; // per-iteration CdlFILE record
      if (libcd.searchFile(c.r[16], c.mem_r32(c.r[17]), table.raSearch)) {
        c.mem_w32(c.r[18], libcd.posToInt(c.r[16], table.raPosToInt));
        c.mem_w32(c.r[18] + 4, c.mem_r32(c.r[16] + 4));
      } else {
        c.r[4] = kNotFoundFmt;
        c.r[5] = c.mem_r32(c.r[17]);
        c.r[31] = table.raPrintf;
        psx::cpu::callGuestNow(c, __func__, kGuestPrintf);
      }
      c.r[18] += 8;
      c.r[17] += 4;
      c.r[19] += 1;
      c.r[20] += 24;
    } while ((int32_t)c.r[19] < (int32_t)table.count);
  }

  // Three XA singletons — CdlFILE at sp+408, LBA to the scratchpad slots.
  for (const auto &xa : kXaFiles) {
    c.r[16] = S + 408;
    c.r[17] = xa.name;
    if (libcd.searchFile(c.r[16], c.r[17], xa.raSearch)) {
      c.mem_w32(xa.dest, libcd.posToInt(c.r[16], xa.raPosToInt));
    } else {
      c.r[4] = kNotFoundFmt;
      c.r[5] = c.r[17];
      c.r[31] = xa.raPrintf;
      psx::cpu::callGuestNow(c, __func__, kGuestPrintf);
    }
  }
  cfg_logi("start.bin", "pc_faithful file table built via libcd (fiber body)");

  // SM loop (L_80106744): the state constants live in the s-regs — spawnAndWait's prologue spills
  // them (s0=3, s1=2, s2=1, s3=r19 left at 5 by loop 3's exit — the RE'd live values).
  c.r[16] = 3;
  c.r[17] = 2;
  c.r[18] = 1;
  uint32_t task = c.mem_r32(CUR_TASK);
  c.mem_w16(task + 0x48, 0);
  c.mem_w16(task + 0x4A, 0);
  for (;;) {
    task = c.mem_r32(CUR_TASK);
    switch (c.mem_r16(task + 0x48)) {
    case 0: // L_8010678C: spawn task-1 texgroup preload
      c.mem_w16(task + 0x48, 1);
      c.mem_w16(task + 0x4A, 0);
      c.r[4] = 0x80044F58u;
      c.r[5] = 0;
      c.r[6] = 0;
      c.r[7] = 0;
      c.r[31] = 0x801067A8u;
      sched.spawnAndWait(0x80044F58u, 0, 0, 0); // parks here once per wait frame
      break;
    case 1: // L_801067B0: spawn task-1 stage-1 preload
      c.mem_w16(task + 0x48, 2);
      c.r[4] = 0x8004514Cu;
      c.r[5] = 1;
      c.r[6] = 1;
      c.r[7] = 0;
      c.r[31] = 0x801067CCu;
      sched.spawnAndWait(0x8004514Cu, 1, 1, 0);
      break;
    case 2:
      c.mem_w16(task + 0x48, 3);
      break;
    case 3: // L_801067DC: stage swap to DEMO — parks the fiber; the stanza cancels on entry rewrite
      c.r[4] = 1;
      c.r[31] = 0x801067E4u;
      psx::cpu::callGuestNow(c, __func__, 0x80052078u);
      break;
    default:
      break;
    }
    c.r[4] = 1;
    c.r[31] = 0x801067ECu; // L_801067E4 trailing per-iteration yield
    sched.yieldPrim(1);
  }
}
