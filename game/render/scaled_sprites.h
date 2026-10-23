// game/render/scaled_sprites.h — FUN_80027A4C, the scaled-sprite writer the FX sprite emitters stamp through.
// a0 = a list of 8-byte sprite records, a1 = texture page (high half) and CLUT (low half).
#pragma once

#include "emit_memory.h"

#include <cstdint>

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba::render {

class ScaledSprites {
public:
  static constexpr std::uint32_t kWriter = 0x80027A4Cu;
  static constexpr std::uint32_t kRecordBytes = 8u;
  static constexpr std::uint32_t kPacketBytes = 0x28u;

  struct Stamp {
    std::uint32_t records;
    std::uint32_t textureClut; // a1
    std::uint32_t ot;
  };
  // The writer over `memory`; over the guest it spills s0 and s1 below sp as the guest body does.
  static std::uint32_t stamp(const psx::present::EmitMemory &memory, const Stamp &stamp);
  // The records `stamp` draws from `records`, through the one carrying an end bit.
  static std::uint32_t recordCount(const psx::present::EmitMemory &memory, std::uint32_t records);

  static void registerOverrides(native::NativeOverrideCatalog &natives);
};

} // namespace tomba::render
