// engine_cd_loader.cpp — sector copies for the engine loaders; the read descriptor is FUN_8001D7C4's.
#include "cd/engine_cd_loader.h"

#include "core.h"
#include "game.h"

#include <lucent/log.h>

#include <array>
#include <span>

namespace tomba {

namespace {

constexpr int V0 = 2;
constexpr std::uint32_t kSectorBytes = 2048u;
constexpr std::uint32_t kLastSectorTracker = 0x800BE0E0u;
constexpr std::uint32_t kReadLba = 0x1F8001F0u;
constexpr std::uint32_t kReadWords = 0x1F8001F4u;
constexpr std::uint32_t kReadDest = 0x1F8001F8u;

struct ReadBytesArgs {
  std::uint32_t lba;
  std::uint32_t dest;
  std::uint32_t bytes;
  std::uint32_t &done;
};
// Copies `bytes` from consecutive sectors at `lba`; returns the sectors read.
std::uint32_t readBytes(Core &core, const ReadBytesArgs &args) {
  auto [lba, dest, bytes, done] = args;
  std::array<std::uint8_t, kSectorBytes> sector{};
  std::uint32_t sectors = 0;
  done = 0;
  for (; done < bytes; ++sectors) {
    if (!core.game->disc.readSector(lba + sectors, sector.data())) {
      break;
    }
    const std::uint32_t n = bytes - done < kSectorBytes ? bytes - done : kSectorBytes;
    core.mem_write(dest + done, std::span(sector).first(n), psx::cpu::ExecutableWriteSource::ModuleLoad);
    done += n;
  }
  return sectors;
}

} // namespace

bool EngineCdLoader::readBlocks(std::uint32_t blocks, std::uint32_t lba, std::uint32_t dest) {
  std::array<std::uint8_t, kSectorBytes> sector{};
  for (std::uint32_t i = 0; i < blocks; ++i) {
    if (!core_.game->disc.readSector(lba + i, sector.data())) {
      return false;
    }
    core_.mem_write(dest + i * kSectorBytes, sector, psx::cpu::ExecutableWriteSource::ModuleLoad);
  }
  lucent::debug("cd", "read {} blk @ LBA {} -> 0x{:08X}", blocks, lba, dest);
  return true;
}

void EngineCdLoader::loadFile(std::uint32_t dest, std::uint32_t lba, std::uint32_t size) {
  std::uint32_t done = 0;
  const std::uint32_t sectors = readBytes(core_, {.lba = lba, .dest = dest, .bytes = size, .done = done});
  if (sectors) {
    core_.mem_w32(kLastSectorTracker, lba + sectors - 1);
  }
  lucent::debug("cd", "loadfile {} B @ LBA {} -> 0x{:08X} ra=0x{:08X}", size, lba, dest, core_.r[31]);
}

// The guest body loops until the per-sector callback FUN_8001D7C4 drains the word count; here the
// words land at once and the descriptor is left as the callback leaves it.
void EngineCdLoader::asyncRead() {
  const std::uint32_t lba = core_.mem_r32(kReadLba);
  const std::uint32_t words = core_.mem_r32(kReadWords);
  const std::uint32_t dest = core_.mem_r32(kReadDest);
  std::uint32_t done = 0;
  const std::uint32_t sectors = readBytes(core_, {.lba = lba, .dest = dest, .bytes = words * 4u, .done = done});
  core_.mem_w32(kReadWords, 0);
  core_.mem_w32(kReadDest, dest + done);
  if (sectors) {
    core_.mem_w32(kLastSectorTracker, lba + sectors - 1);
  }
  lucent::debug("cd", "async read {} words @ LBA {} -> 0x{:08X}", words, lba, dest);
}

void EngineCdLoader::dc40Sync(const Dc40SyncArgs &args) {
  auto [dest, lba, size] = args;
  core_.mem_w32(kReadDest, dest);
  core_.mem_w32(kReadLba, lba);
  core_.mem_w32(kReadWords, (size + 3u) >> 2);
  asyncRead();
  core_.r[V0] = size;
}

} // namespace tomba
