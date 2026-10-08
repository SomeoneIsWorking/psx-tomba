// start_bin_stage.h — class StartBinStage — task-0's START.BIN file-table builder, the stage-0
// body at overlay guest 0x8010649C. It resolves the ~36 disc filenames baked into START.BIN into
// the guest {LBA,size} tables, runs the boot preloads, and ends at the stage swap into DEMO.
//
// Two bodies, one per execution model (no inline native_sync branching — user rule 2026-07-04:
// call one of two named methods):
//   runNative()   — native_sync: host ISO9660 lookups (LibcdDirCache keeps libcd's guest cache
//                   byte-identical), inline preloads, task-1 slot closed as if it ran and
//                   completed. Returns with the task at sm[0x48]==3; the caller owns the stage
//                   swap (Engine::startStage(1)).
//   runFaithful() — pc_faithful: the COMPLETE guest task body as a native port on a PcScheduler
//                   fiber (faithful-execution model, docs/faithful-execution.md): locals in the
//                   real guest frame, s-registers live through the loops, libcd itself through
//                   LibcdNative, suspension inside PcScheduler primitives. Never returns — its
//                   sm==3 arm parks in guest FUN_80052078 until the stanza cancels the fiber.
//
// The filename tables and their addresses are fixed by the START.BIN overlay; the faithful body's
// call-site ra constants are part of the port's identity (RE: authenticated overlay evidence
// 0x8010649C..0x80106728).
#pragma once
#include <cstdint>
class Asset;
class Core;

class StartBinStage {
public:
  StartBinStage(Core &core, Asset &asset) : core_(core), asset_(asset) {}

  void runNative();
  [[noreturn]] void runFaithful();

private:
  // Baked 16x1 pixel strip inside START.BIN, uploaded to VRAM(944,256) before the file table.
  static constexpr uint32_t kLoadImageSrc = 0x80106878u;

  // Resolve the guest filename at `name_ptr` through the host ISO9660 reader. Logs a miss.
  bool resolveViaIso9660(uint32_t name_ptr, uint32_t *lba, uint32_t *size);
  // Every filename table plus the three XA singletons, written to their guest destinations.
  void resolveFileTablesNative();
  // Stage-0 SM advance + boot texgroup preload with the task-1 slot closed synchronously.
  void advanceWithBootPreload();

  Core &core_;
  Asset &asset_;
};
