#include "packet_pool.h"

#include "core.h"

#include <cstdlib>
#include <lucent/log.h>

namespace tomba1 {

std::uint32_t PacketPool::passBytes(Core &core) {
  return ((core.mem_r32(kCursor) & 0x00FFFFFFu) - kBase) % kPassBytes;
}

std::uint32_t PacketPool::reserve(Core &core, std::uint32_t bytes) {
  const std::uint32_t cursor = core.mem_r32(kCursor);
  const std::uint32_t offset = (cursor & 0x00FFFFFFu) - kBase;
  const std::uint32_t pass = offset / kPassBytes;
  if (pass >= kPasses || offset + bytes > (pass + 1u) * kPassBytes) {
    lucent::error("tomba1-packet-pool",
                  "a {}-byte packet at cursor 0x{:08X} passes the end of pass {} ({} bytes each from 0x{:08X})",
                  bytes,
                  cursor,
                  pass,
                  kPassBytes,
                  kBase);
    std::abort();
  }
  return cursor;
}

} // namespace tomba1
