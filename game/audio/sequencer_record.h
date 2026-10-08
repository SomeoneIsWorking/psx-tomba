// game/audio/sequencer_record.h — the per-(sequence, channel) record lens shared by the Sequencer's
// leaf bodies. Layout and field roles are reverse-engineered; see docs/engine_re.md's libsnd
// sections.
#pragma once

#include "audio/libsnd_globals.h"
#include "core.h"

#include <cstdint>

namespace tomba::audio::record {

// The per-CHANNEL state block is reached through the sequence's own pointer table, not through the
// sound driver's global cluster: base = *libsnd::kSeqPtrArray[seq] + chStride(chan).
inline constexpr uint32_t kFlags = 152u;
inline constexpr uint32_t kStride = 176u;

// The guest sign-extends every sequence/channel index with `sll rX,16 / sra rX,16`.
inline int32_t sext16(uint32_t v) {
  return (int32_t)(int16_t)(uint16_t)v;
}

// The guest computes the byte stride as `(chan*11)<<4`, which is bit-identical to `chan*176` under
// mod-2^32 arithmetic.
inline uint32_t chStride(int32_t chan) {
  return (uint32_t)(chan * 11) << 4;
}

// The 4-byte-stride `kSeqPtrArray[seq]` slot address.
inline uint32_t seqPtrSlot(uint32_t seqRaw) {
  return libsnd::kSeqPtrArray + (uint32_t)(sext16(seqRaw) << 2);
}

// Named accessors exist only for fields more than one leaf shares; the register-literal leaves keep
// raw `mem_rXX(base + N)` accesses so they read exactly as the guest does.
struct ChannelRecord {
  Core *c;
  uint32_t base;

  uint32_t flags() const {
    return c->mem_r32(base + kFlags);
  }
  void setFlags(uint32_t v) {
    c->mem_w32(base + kFlags, v);
  }
  void clearFlagBits(uint32_t mask) {
    setFlags(flags() & ~mask);
  }

  void setBusy(uint8_t v) {
    c->mem_w8(base + 20u, v);
  }

  uint16_t volL() const {
    return c->mem_r16(base + 88u);
  }
  uint16_t volR() const {
    return c->mem_r16(base + 90u);
  }
  uint32_t snapshotTargetLPtr() const {
    return base + 92u;
  }
  uint32_t snapshotTargetRPtr() const {
    return base + 94u;
  }
};

} // namespace tomba::audio::record