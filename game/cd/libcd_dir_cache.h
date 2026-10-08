// libcd_dir_cache.h — class LibcdDirCache — the native ISO9660-backed writer of libcd's guest
// directory cache: ports of FUN_8008BBE8 (CdNewMedia) and FUN_8008BF50 (CdCacheFile).
//
// The native_sync START.BIN stage resolves files through the host ISO9660 reader instead of
// libcd, but the original CdSearchFile populates libcd's in-memory directory cache as a side
// effect. This class reproduces exactly those guest writes from the same disc sectors, so guest RAM
// stays byte-identical to the faithful path and a later guest dispatch into CdSearchFile
// short-circuits its new-media branch. LibcdNative (libcd_native.h) is the sibling for the
// pc_faithful path: it runs the guest libcd code itself.
//
// RE: Ghidra headless decomp of FUN_8008BBE8 / FUN_8008BF50 (SCEI library range
// 0x80080000..0x8009E000). ISO9660 field offsets are the standard PVD / path-table-L / dir-record
// layout.
#pragma once
#include <cstdint>
class Core;

class LibcdDirCache {
public:
  explicit LibcdDirCache(Core &core) : core_(core) {}

  // FUN_8008BBE8 CdNewMedia: read the PVD at LBA 16, follow it to the L path table, and walk its
  // entries into the path-table cache. Leaves the current cached dir at 0 (root loaded).
  void newMedia();

  // FUN_8008BF50 CdCacheFile(dir_idx): read the directory sector at path_cache[dir_idx-1].extent
  // and walk its records into the file-entry cache. Idempotent when dir_idx is already cached.
  void cacheFile(uint32_t dir_idx);

  // CdSearchFile's side effect on the media-generation cookie: copy libcd's counter into the
  // search-owned copy so a later guest CdSearchFile sees the media as already scanned.
  void stampMediaCookie();

private:
  // Guest state populated (libcd data segment):
  static constexpr uint32_t kCurCachedDir = 0x800AC2D4u;   // u32: 0 = root loaded, else 1-based path-table idx
  static constexpr uint32_t kMediaCookieDst = 0x800AC2D8u; // = DAT_800ABFD0 after CdSearchFile
  static constexpr uint32_t kMediaCookieSrc = 0x800ABFD0u; // libcd media-generation counter
  static constexpr uint32_t kPathTableCache = 0x80102D68u; // 128 × 44 B: {index, parent, extent, name[32]}
  static constexpr uint32_t kFileEntryCache = 0x80102768u; // 64 × 24 B: {MSF, size, name}
  static constexpr uint32_t kPathExtentBase = 0x80102D70u; // = kPathTableCache + 8 (extent of slot 0)
  static constexpr uint32_t kSectorScratch = 0x80104368u;  // 2 KB scratch buffer libcd reads into
  static constexpr uint32_t kPathTableStride = 44u;
  static constexpr uint32_t kFileEntryStride = 24u;
  static constexpr int kPathTableSlots = 128;
  static constexpr int kFileEntrySlots = 64;
  static constexpr uint32_t kSectorBytes = 2048u;

  // Read one 2048 B data sector into `out` AND into the guest scratch at kSectorScratch, mirroring
  // the original per-sector read into DAT_80104368. False on read failure.
  bool readSectorIntoScratch(uint32_t lba, uint8_t *out);

  Core &core_;
};
