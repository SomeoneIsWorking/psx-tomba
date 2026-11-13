// game/render/guest_ordering_table.cpp — OrderingTable and PacketPool. See guest_ordering_table.h.
#include "guest_ordering_table.h"

#include "core.h"
#include "core/frame/frame_driver.h"

#include <cstdlib>
#include <lucent/log.h>

namespace tomba::render {

OrderingTable OrderingTable::active(const psx::present::EmitMemory &memory) {
  return OrderingTable(memory, memory.mem_r32(kBasePointer));
}

OrderingTable::OrderingTable(const psx::present::EmitMemory &memory, std::uint32_t base)
    : mMemory(memory), mBase(base) {}

std::uint32_t OrderingTable::head(std::uint32_t bucket) const {
  return mMemory.mem_r32(slot(bucket));
}

void OrderingTable::setHead(std::uint32_t bucket, std::uint32_t packet) const {
  mMemory.mem_w32(slot(bucket), packet);
}

std::uint32_t OrderingTable::link(const LinkArgs &args) const {
  auto [packet, words, bucket] = args;
  const std::uint32_t tag = head(bucket) | (words << 24);
  mMemory.mem_w32(packet, tag);
  setHead(bucket, packet);
  return tag;
}

void OrderingTable::chainToHead(const ChainToHeadArgs &args) const {
  auto [lastPacket, bucket] = args;
  const std::uint32_t oldHead = head(bucket);
  mMemory.mem_w32(lastPacket, (mMemory.mem_r32(lastPacket) & 0xFF000000u) | oldHead);
}

PacketPool::PacketPool(const psx::present::EmitMemory &memory) : mMemory(memory) {}

std::uint32_t PacketPool::cursor() const {
  return mMemory.mem_r32(kCursor);
}

void PacketPool::setCursor(std::uint32_t address) const {
  mMemory.mem_w32(kCursor, address);
}

std::uint32_t PacketPool::reserve(std::uint32_t packet, std::uint32_t bytes) const {
  if ((packet & 0xFFFFFFu) + bytes > (kEnd & 0xFFFFFFu)) {
    lucent::error("tomba-packet-pool",
                  "a {}-byte packet at {:08X} would pass the pool end {:08X} ({} bytes from {:08X})",
                  bytes,
                  packet,
                  kEnd,
                  kEnd - kBase,
                  kBase);
    std::abort();
  }
  return packet;
}

std::uint32_t PacketPool::reserve(std::uint32_t bytes) const {
  return reserve(cursor(), bytes);
}

std::uint32_t PacketPool::allocate(std::uint32_t bytes) const {
  const std::uint32_t packet = reserve(bytes);
  setCursor(packet + bytes);
  return packet;
}

std::uint32_t PacketPool::end() const {
  const std::uint32_t parity = mMemory.mem_r8(kBufferParityAddress) != 0u ? 1u : 0u;
  return (kBase + (parity + 1u) * kStride) & 0xFFFFFFu;
}

bool PacketPool::hasRoom() const {
  return (cursor() & 0xFFFFFFu) < end() - kReserve;
}

bool PacketPool::fits(std::uint32_t packet, std::uint32_t bytes) const {
  return (packet & 0xFFFFFFu) + bytes <= end() - kReserve;
}

} // namespace tomba::render
