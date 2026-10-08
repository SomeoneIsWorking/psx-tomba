// libcd_dir_cache.cpp — see header.
#include "libcd_dir_cache.h"
#include "core.h"
#include "disc.h" // disc_read_sector — native by-LBA disc backend
#include "game.h" // Game::disc

namespace {
uint32_t le32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
uint8_t bcd(int v) {
  return (uint8_t)((v % 10) + ((v / 10) << 4));
}
} // namespace

bool LibcdDirCache::readSectorIntoScratch(uint32_t lba, uint8_t *out) {
  if (!disc_read_sector(&core_.game->disc, lba, out)) {
    return false;
  }
  for (uint32_t i = 0; i < kSectorBytes; i++) {
    core_.mem_w8(kSectorScratch + i, out[i]);
  }
  return true;
}

void LibcdDirCache::newMedia() {
  uint8_t sec[kSectorBytes];
  if (!readSectorIntoScratch(16, sec)) {
    return;
  }
  // "CD001" magic at PVD offset 1 (bytes 1..5).
  if (sec[1] != 'C' || sec[2] != 'D' || sec[3] != '0' || sec[4] != '0' || sec[5] != '1') {
    return;
  }
  // PVD offset 0x8C = L-path-table LBA (u32 LE).
  if (!readSectorIntoScratch(le32(sec + 0x8C), sec)) {
    return;
  }

  // Path-table entry: byte name_len + byte xa_len + u32 extent LE + u16 parent LE + name[name_len]
  // + pad-to-even.
  uint32_t off = 0;
  int i = 0;
  while (off + 8 <= kSectorBytes && sec[off] != 0 && i < kPathTableSlots) {
    const uint8_t name_len = sec[off + 0];
    const uint32_t extent = le32(sec + off + 2);
    const uint8_t parent = sec[off + 6]; // only the low byte is kept (max 128 dirs)

    const uint32_t slot = kPathTableCache + (uint32_t)i * kPathTableStride;
    core_.mem_w32(slot + 0, (uint32_t)(i + 1)); // 1-based dir index
    core_.mem_w32(slot + 4, (uint32_t)parent);
    core_.mem_w32(slot + 8, extent);
    for (uint32_t j = 0; j < name_len; j++) {
      core_.mem_w8(slot + 12 + j, sec[off + 8 + j]);
    }
    core_.mem_w8(slot + 12 + name_len, 0);

    off += 8u + (uint32_t)name_len + ((uint32_t)name_len & 1u);
    i++;
  }
  // Sentinel: the next slot's parent field is cleared (loop-exit marker of FUN_8008BBE8).
  if (i < kPathTableSlots) {
    core_.mem_w32(kPathTableCache + (uint32_t)i * kPathTableStride + 4, 0);
  }
  core_.mem_w32(kCurCachedDir, 0);
}

void LibcdDirCache::cacheFile(uint32_t dir_idx) {
  if (dir_idx == core_.mem_r32(kCurCachedDir)) {
    return;
  }
  const uint32_t dir_lba = core_.mem_r32(kPathExtentBase + (dir_idx - 1u) * kPathTableStride);
  uint8_t sec[kSectorBytes];
  if (!readSectorIntoScratch(dir_lba, sec)) {
    core_.mem_w32(kCurCachedDir, dir_idx);
    return;
  }

  uint32_t off = 0;
  int i = 0;
  while (off < kSectorBytes && sec[off] != 0 && i < kFileEntrySlots) {
    const uint32_t rec_len = sec[off + 0];
    const uint32_t extent = le32(sec + off + 2);
    const uint32_t size = le32(sec + off + 10);
    const uint8_t fn_len = sec[off + 32];

    // FUN_8008A00C: LBA -> BCD MSF (sector = LBA + 150, split into min/sec/frame at 75 fps).
    const int t = (int)extent + 150;
    const int frame = t % 75;
    const int rem = t / 75;
    const int ssec = rem % 60;
    const int min = rem / 60;

    const uint32_t slot = kFileEntryCache + (uint32_t)i * kFileEntryStride;
    core_.mem_w8(slot + 0, bcd(min));
    core_.mem_w8(slot + 1, bcd(ssec));
    core_.mem_w8(slot + 2, bcd(frame));
    core_.mem_w32(slot + 4, size);
    if (i == 0) {
      core_.mem_w32(slot + 8, 0x0000002Eu); // "." — DAT_8001C528 written as one u32
    } else if (i == 1) {
      core_.mem_w32(slot + 8, 0x00002E2Eu); // ".." — DAT_8001C52C u32 + DAT_8001C52E u16 0
      core_.mem_w16(slot + 10, 0);
    } else {
      for (uint32_t j = 0; j < fn_len; j++) {
        core_.mem_w8(slot + 8 + j, sec[off + 33 + j]);
      }
      core_.mem_w8(slot + 8 + fn_len, 0);
    }

    off += rec_len;
    i++;
  }
  // Sentinel: the next name slot's first byte is nulled ((u8)(&DAT_80102770 + i*0xc) = 0).
  if (i < kFileEntrySlots) {
    core_.mem_w8(kFileEntryCache + (uint32_t)i * kFileEntryStride + 8, 0);
  }
  core_.mem_w32(kCurCachedDir, dir_idx);
}

void LibcdDirCache::stampMediaCookie() {
  core_.mem_w32(kMediaCookieDst, core_.mem_r32(kMediaCookieSrc));
}
