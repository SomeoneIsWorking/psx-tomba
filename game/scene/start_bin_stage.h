// start_bin_stage.h — class StartBinStage — task-0's START.BIN file-table builder, the stage-0
// body at overlay guest 0x8010649C. It resolves the ~36 disc filenames baked into START.BIN into
// the guest {LBA,size} tables, runs the boot preloads, and ends at the stage swap into DEMO.
//
// runNative() looks the names up on the host ISO9660 (LibcdDirCache keeps libcd's guest cache
// byte-identical), runs the preloads inline and closes task-1's slot as if it had run; it returns
// with the task at sm[0x48]==3 and the caller owns the stage swap (Engine::startStage(1)).
#pragma once
#include <cstdint>
namespace tomba {

class Asset;

} // namespace tomba
class Core;

namespace tomba {

class StartBinStage {
public:
  StartBinStage(Core &core, Asset &asset) : core_(core), asset_(asset) {}

  void runNative();

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

} // namespace tomba
