// game/render/guest_ordering_table.cpp — OrderingTable and PacketPool. See guest_ordering_table.h.
#include "guest_ordering_table.h"

#include "core.h"

namespace tomba2::render {

OrderingTable OrderingTable::active(const EmitMemory &memory) {
  return OrderingTable(memory, memory.mem_r32(kBasePointer));
}

OrderingTable::OrderingTable(const EmitMemory &memory, std::uint32_t base) : mMemory(memory), mBase(base) {}

std::uint32_t OrderingTable::head(std::uint32_t bucket) const {
  return mMemory.mem_r32(slot(bucket));
}

void OrderingTable::setHead(std::uint32_t bucket, std::uint32_t packet) const {
  mMemory.mem_w32(slot(bucket), packet);
}

std::uint32_t OrderingTable::link(std::uint32_t packet, std::uint32_t words, std::uint32_t bucket) const {
  const std::uint32_t tag = head(bucket) | (words << 24);
  mMemory.mem_w32(packet, tag);
  setHead(bucket, packet);
  return tag;
}

void OrderingTable::chainToHead(std::uint32_t lastPacket, std::uint32_t bucket) const {
  const std::uint32_t oldHead = head(bucket);
  mMemory.mem_w32(lastPacket, (mMemory.mem_r32(lastPacket) & 0xFF000000u) | oldHead);
}

PacketPool::PacketPool(const EmitMemory &memory) : mMemory(memory) {}

std::uint32_t PacketPool::cursor() const {
  return mMemory.mem_r32(kCursor);
}

void PacketPool::setCursor(std::uint32_t address) const {
  mMemory.mem_w32(kCursor, address);
}

std::uint32_t PacketPool::allocate(std::uint32_t bytes) const {
  const std::uint32_t packet = cursor();
  setCursor(packet + bytes);
  return packet;
}

} // namespace tomba2::render
