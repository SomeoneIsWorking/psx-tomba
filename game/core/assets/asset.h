// game/core/asset.h — class Asset — PC-native ASSET LOADING subsystem owned by Engine
// (LZ decompress + texture-group unpack + CPU→VRAM upload + stage-0/1 preload chain).
//
// PROPER OOP: one instance per Core, embedded on Engine (`eng(c).asset`). Back-pointer `core`
// wired at Core construction time (same pattern as Font / Placement / Pool / GraphicsBind).
// Subsystem, not a math library — Asset owns the engine's loading domain (staging buffers, VRAM
// upload, the boot preload chain) even though the class has no fields today. See CLAUDE.md
// "REAL C++ CLASSES" — default = instance for subsystems; static is only for pure-math libraries.
//
// SCOPE: the LZ image decompressor (FUN_80044D8C), the texture-group unpacker (FUN_80044E84),
// the texture-group loader orchestration (FUN_80044F58), the CPU→VRAM upload (FUN_80081218), and
// the stage-0/stage-1 boot PRELOAD chain (preloadTexgroup / preloadStage1).
//
// Was the free functions ov_lz_decompress / ov_unpack_group / ov_load_texgroup / ov_upload_image
// + free helpers preload_texgroup / preload_stage1, each taking its arguments via MIPS taxi
// parameters (c->r[4/5/6/7]) or as C args. Now real instance methods with explicit typed args.
#ifndef ENGINE_ASSET_H
#define ENGINE_ASSET_H
#include <stdint.h>
class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba {

class Asset {
public:
  // registerOverrides(): installs the resident native owner of FUN_80045558 (indexed file load into
  // the shared AREA slot) so every guest caller, including the pause-menu card page, publishes the
  // loaded OPN/CRD code image through tomba::native::loadAreaSlotFile.
  static void registerOverrides(native::NativeOverrideCatalog &natives);

  Core *core = nullptr;

  struct LzDecompressArgs {
    uint32_t desc;
    uint32_t dst;
    uint32_t src0;
    uint32_t srclen;
  };
  // lzDecompress(desc, dst, src, srclen): FUN_80044D8C — the LZ image decompressor. Returns
  //   bytes written into dst. desc[+4] = row stride (s16) used for row-neighbour back-refs via
  //   the offset table @0x800153C8.
  uint32_t lzDecompress(const LzDecompressArgs &args);

  struct GroupRangeArgs {
    uint32_t tablePtr;
    uint32_t anchorEnd;
  };
  // unpackGroup(tablePtr, anchorEnd): FUN_80044E84 — texture-group unpacker. Iterates the
  //   descriptor table (count @+0, 12-byte entries after +4), decompressing each image into
  //   transient scratch ending at anchorEnd, then uploading (uploadImage) to its VRAM (x,y).
  void unpackGroup(const GroupRangeArgs &args);

  // unpackGroupFaithful(tablePtr, anchorEnd): FUN_80044E84 with full guest-stack discipline
  //   (frame descent 48, live s-reg/ra spills, libgs LoadImage/DrawSync dispatched at the live
  //   guest sp) — the pc_faithful task-1 path, byte-exact to the substrate body under strict SBS.
  void unpackGroupFaithful(const GroupRangeArgs &args);

  // loadTexgroup(): FUN_80044F58 — texture-group LOADER orchestration. mode/set inputs come from
  //   the current task struct at *(0x1F800138u) — [+0x6D]=mode, [+0x6E]=set — so no explicit
  //   args. CD-loads header + archive, unpacks, copies the 42-word metadata, then (mode==0) does
  //   the terminal task yield (does not return mid-game).
  void loadTexgroup();

  struct UploadImageArgs {
    uint32_t desc;
    uint32_t src;
  };
  // uploadImage(desc, src): FUN_80081218 — PC-native CPU→VRAM upload. desc = { x:s16, y:s16,
  //   w:s16, h:s16 }; src = w*h contiguous 16-bit pixels (row-major). Bypasses the PSX
  //   GsSortObject ring — writes straight into native VRAM.
  void uploadImage(const UploadImageArgs &args);

  struct PreloadTexgroupArgs {
    uint32_t mode;
    uint32_t set;
  };
  // preloadTexgroup(mode, set): stage-0/stage-1 texgroup PRELOAD — synchronous CD read +
  //   loadTexgroup path with explicit mode/set (not read from the task struct). Used by the
  //   native_stage0_sm boot preload chain in engine.cpp and by demo.cpp's area-load.
  void preloadTexgroup(const PreloadTexgroupArgs &args);

  // FUN_8004514C — SWDATA + DAT load, shared texgroup sub-load, relocation table, cel/sprite VRAM build.
  void preloadStage1();

  struct LoadDescriptorChunkArgs {
    uint32_t descIdx;
    uint32_t slot;
  };
  // loadDescriptorChunk(descIdx, slot): FUN_80045258 — reads one CD sub-chunk described by the
  //   byte-offset table @0x800FB170 (word-indexed: descTable[descIdx] = start offset,
  //   descTable[descIdx+1] = end offset; size = end-start) into the destination pointer already
  //   latched at module-pointer table @0x800ECF58[slot] (word-indexed: a fixed engine slot, e.g.
  //   the collision-grid buffer at slot 47/0x2f, or the area-8 extra-texgroup buffer at slot 8).
  //   Sector = DAT_800BE100 (the area's disc extent base) + (descTable[descIdx] >> 11). Pure
  //   leaf: computes args and issues one EngineCdLoader::dc40Sync read, byte-identical to the substrate.
  void loadDescriptorChunk(const LoadDescriptorChunkArgs &args);
};

} // namespace tomba
#endif
