// game/render/call_site_objects.h — the drawing objects of a writer reached from many call sites.
#pragma once

#include <cstdint>
#include <unordered_map>

namespace tomba::render {

// One call is the n-th from its return address in the logic frame; n sits above the address's low 24 bits.
class CallSiteObjects {
public:
  std::uint32_t object(std::uint32_t returnAddress) {
    const std::uint32_t nth = calls_[returnAddress]++;
    return (nth << kSiteBits) | (returnAddress & kSiteMask);
  }
  // A new logic frame: every site counts from zero again.
  void beginFrame() {
    calls_.clear();
  }

private:
  static constexpr std::uint32_t kSiteBits = 24u;
  static constexpr std::uint32_t kSiteMask = (1u << kSiteBits) - 1u;

  std::unordered_map<std::uint32_t, std::uint32_t> calls_;
};

} // namespace tomba::render
