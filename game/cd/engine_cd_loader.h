// engine_cd_loader.h — Tomba! 2's engine file loaders, served synchronously from the disc.
#pragma once

#include <cstdint>

class Core;

namespace tomba {

// FUN_8008C1EC, FUN_8001DB8C, FUN_8001D940 and FUN_8001DC40. The bytes land in guest RAM before the call returns and
// the guest's own position tracker and read descriptor are left as the IRQ-driven reader leaves them.
class EngineCdLoader {
public:
  explicit EngineCdLoader(Core &core) : core_(core) {}

  // FUN_8008C1EC's read: `blocks` whole sectors from `lba`; false on a disc read failure.
  bool readBlocks(std::uint32_t blocks, std::uint32_t lba, std::uint32_t dest);
  // FUN_8001DB8C(dest, lba, size): ceil(size/2048) sectors from `lba`, exactly `size` bytes.
  void loadFile(std::uint32_t dest, std::uint32_t lba, std::uint32_t size);
  // FUN_8001D940 from the scratchpad read descriptor (lba 0x1F8001F0, words 0x1F8001F4, dest 0x1F8001F8).
  void asyncRead();
  struct Dc40SyncArgs {
    std::uint32_t dest;
    std::uint32_t lba;
    std::uint32_t size;
  };
  // FUN_8001DC40(dest, lba, size): fills the descriptor and runs asyncRead; v0 = size.
  void dc40Sync(const Dc40SyncArgs &args);

private:
  Core &core_;
};

} // namespace tomba
