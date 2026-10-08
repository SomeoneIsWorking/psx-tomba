// game/render/guest_ordering_table.h — the guest's ordering table and packet pool, as its render code uses them.
//
// Every guest render leaf takes a packet from the bump pool whose cursor lives at 0x800BF544, and links
// it into a bucket of the OT whose base the frame driver publishes at 0x800ED8C8. The link is libgpu's
// AddPrim, inlined: the packet's tag takes the bucket's old head with its word count in the top byte,
// and the bucket's head becomes the packet. Depths reach a bucket through one log-scale compression.
#pragma once

#include <cstdint>

class Core;

namespace tomba2::render {

class OrderingTable {
public:
  // Guest word holding the active OT base.
  static constexpr std::uint32_t kBasePointer = 0x800ED8C8u;
  // Guest code addresses kBasePointer as this page - 10040, with the page held in a register.
  static constexpr std::uint32_t kBasePointerPage = 0x800F0000u;
  static constexpr std::uint32_t kBucketCount = 0x800u;
  static constexpr std::int32_t kNearestBucket = 4;
  static constexpr std::int32_t kFarthestBucket = 0x7FF;
  static constexpr std::int32_t kNoBucket = -1;

  // The OT the frame driver published.
  static OrderingTable active(Core &core);
  // An OT whose base a caller handed over in a register.
  OrderingTable(Core &core, std::uint32_t base);

  std::uint32_t base() const {
    return mBase;
  }
  std::uint32_t slot(std::uint32_t bucket) const {
    return mBase + bucket * 4u;
  }
  std::uint32_t head(std::uint32_t bucket) const;
  void setHead(std::uint32_t bucket, std::uint32_t packet) const;

  // AddPrim: returns the tag word written to the packet.
  std::uint32_t link(std::uint32_t packet, std::uint32_t words, std::uint32_t bucket) const;
  // Points a packet chain's last tag at the bucket's head, keeping that tag's word count.
  void chainToHead(std::uint32_t lastPacket, std::uint32_t bucket) const;

  // The log-scale depth compression: bits above 10 pick a band that both shifts and offsets the depth.
  static constexpr std::int32_t compressDepth(std::int32_t depth) {
    const std::int32_t band = depth >> 10;
    return (depth >> (band & 31)) + band * 0x200;
  }
  // [kNearestBucket, kFarthestBucket], the gate most leaves apply.
  static constexpr bool inDepthRange(std::int32_t bucket) {
    return static_cast<std::uint32_t>(bucket) - static_cast<std::uint32_t>(kNearestBucket) <
           static_cast<std::uint32_t>(kFarthestBucket - kNearestBucket + 1);
  }
  // (kNearestBucket, kFarthestBucket): the A00 field GT3/GT4 leaves drop both end buckets too.
  static constexpr bool inDepthRangeExclusive(std::int32_t bucket) {
    return bucket > kNearestBucket && bucket < kFarthestBucket;
  }

private:
  Core *mCore;
  std::uint32_t mBase;
};

static_assert(OrderingTable::kBasePointerPage - 10040u == OrderingTable::kBasePointer);

class PacketPool {
public:
  // Guest word holding the pool's next free byte.
  static constexpr std::uint32_t kCursor = 0x800BF544u;
  // Guest code addresses kCursor as this page - 2748, with the page held in a register.
  static constexpr std::uint32_t kCursorPage = 0x800C0000u;

  explicit PacketPool(Core &core);

  std::uint32_t cursor() const;
  void setCursor(std::uint32_t address) const;
  // Bumps the cursor past `bytes` and returns the packet it pointed at.
  std::uint32_t allocate(std::uint32_t bytes) const;

private:
  Core *mCore;
};

static_assert(PacketPool::kCursorPage - 2748u == PacketPool::kCursor);

} // namespace tomba2::render
