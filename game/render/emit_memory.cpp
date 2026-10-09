// game/render/emit_memory.cpp — HostMemory. See emit_memory.h.
#include "emit_memory.h"

#include <lucent/log.h>

#include <cstdlib>
#include <cstring>
#include <utility>

namespace tomba2::render {
namespace {

// Guest addresses reach the same byte through KUSEG, KSEG0 and KSEG1.
constexpr std::uint32_t kPhysicalMask = 0x1FFFFFFFu;

std::uint32_t physical(std::uint32_t address) {
  return address & kPhysicalMask;
}

} // namespace

const HostMemory::Range *HostMemory::find(std::uint32_t address) const {
  const std::uint32_t at = physical(address);
  for (const Range &range : ranges_) {
    if (at - range.base < range.bytes.size()) {
      return &range;
    }
  }
  return nullptr;
}

HostMemory::Range *HostMemory::find(std::uint32_t address) {
  return const_cast<Range *>(std::as_const(*this).find(address));
}

void HostMemory::add(std::uint32_t address, std::vector<std::byte> bytes) {
  const std::uint32_t base = physical(address);
  for (const Range &range : ranges_) {
    if (base < range.base + range.bytes.size() && range.base < base + bytes.size()) {
      lucent::error("emit-memory", "host range at 0x{:08X} overlaps the one at 0x{:08X}", address, range.base);
      std::abort();
    }
  }
  ranges_.push_back(Range{base, std::move(bytes)});
}

void HostMemory::provide(std::uint32_t address, std::span<const std::byte> bytes) {
  add(address, std::vector<std::byte>(bytes.begin(), bytes.end()));
}

void HostMemory::zero(std::uint32_t address, std::uint32_t size) {
  add(address, std::vector<std::byte>(size));
}

bool HostMemory::read(std::uint32_t address, void *out, std::uint32_t size) const {
  const Range *range = find(address);
  if (range == nullptr) {
    return false;
  }
  const std::uint32_t offset = physical(address) - range->base;
  if (offset + size > range->bytes.size()) {
    lucent::error("emit-memory", "a {}-byte read at 0x{:08X} runs past its host range", size, address);
    std::abort();
  }
  std::memcpy(out, range->bytes.data() + offset, size);
  return true;
}

void HostMemory::write(std::uint32_t address, const void *in, std::uint32_t size) {
  Range *range = find(address);
  if (range == nullptr) {
    lucent::error("emit-memory", "a render wrote {} bytes to 0x{:08X}, outside its host ranges", size, address);
    std::abort();
  }
  const std::uint32_t offset = physical(address) - range->base;
  if (offset + size > range->bytes.size()) {
    lucent::error("emit-memory", "a {}-byte write at 0x{:08X} runs past its host range", size, address);
    std::abort();
  }
  std::memcpy(range->bytes.data() + offset, in, size);
}

std::span<const std::byte> HostMemory::view(std::uint32_t address, std::uint32_t size) const {
  const Range *range = find(address);
  const std::uint32_t offset = range != nullptr ? physical(address) - range->base : 0u;
  if (range == nullptr || offset + size > range->bytes.size()) {
    lucent::error("emit-memory", "no host range holds {} bytes at 0x{:08X}", size, address);
    std::abort();
  }
  return std::span<const std::byte>(range->bytes).subspan(offset, size);
}

} // namespace tomba2::render
