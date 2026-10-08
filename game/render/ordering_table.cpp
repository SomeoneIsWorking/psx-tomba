// game/render/ordering_table.cpp — OrderingTable and PacketPool. See ordering_table.h.
#include "ordering_table.h"

#include "core.h"

namespace tomba2::render {

OrderingTable OrderingTable::active(Core &core) {
  return OrderingTable(core, core.mem_r32(kBasePointer));
}

OrderingTable::OrderingTable(Core &core, std::uint32_t base) : mCore(&core), mBase(base) {}

std::uint32_t OrderingTable::head(std::uint32_t bucket) const {
  return mCore->mem_r32(slot(bucket));
}

void OrderingTable::setHead(std::uint32_t bucket, std::uint32_t packet) const {
  mCore->mem_w32(slot(bucket), packet);
}

std::uint32_t OrderingTable::link(std::uint32_t packet, std::uint32_t words, std::uint32_t bucket) const {
  const std::uint32_t tag = head(bucket) | (words << 24);
  mCore->mem_w32(packet, tag);
  setHead(bucket, packet);
  return tag;
}

void OrderingTable::chainToHead(std::uint32_t lastPacket, std::uint32_t bucket) const {
  const std::uint32_t oldHead = head(bucket);
  mCore->mem_w32(lastPacket, (mCore->mem_r32(lastPacket) & 0xFF000000u) | oldHead);
}

PacketPool::PacketPool(Core &core) : mCore(&core) {}

std::uint32_t PacketPool::cursor() const {
  return mCore->mem_r32(kCursor);
}

void PacketPool::setCursor(std::uint32_t address) const {
  mCore->mem_w32(kCursor, address);
}

std::uint32_t PacketPool::allocate(std::uint32_t bytes) const {
  const std::uint32_t packet = cursor();
  setCursor(packet + bytes);
  return packet;
}

} // namespace tomba2::render
