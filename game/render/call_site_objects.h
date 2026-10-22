// game/render/call_site_objects.h — the drawing objects of a writer reached from many call sites.
#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

namespace tomba::render {

// Counted per logic frame; the counts keep their storage, so a steady stream of frames allocates nothing.
class CallSiteObjects {
public:
  // The n-th call from `returnAddress` in the logic frame; n sits above the address's low 24 bits.
  std::uint32_t object(std::uint32_t returnAddress) {
    const std::uint32_t nth = count(returnAddress, kNoList);
    return (nth << kSiteBits) | (returnAddress & kSiteMask);
  }
  // The n-th call from `returnAddress` drawing `list` in the logic frame, so a call keeps its object while
  // the site's other lists come and go. The site is its order of first call in the session.
  std::uint32_t object(std::uint32_t returnAddress, std::uint32_t list) {
    // Past the field widths calls share an object, which the composer keeps whole as ambiguous.
    const std::uint32_t nth = std::min(count(returnAddress, list), kListNthMask);
    const std::uint32_t site = std::min(siteOrdinal(returnAddress), kListSiteMask);
    return kListObject | (site << kListSiteShift) | (nth << kListNthShift) | ((list & kRamMask) >> 2);
  }
  // A new logic frame: every site counts from zero again.
  void beginFrame() {
    counts_.clear();
  }

private:
  static constexpr std::uint32_t kSiteBits = 24u;
  static constexpr std::uint32_t kSiteMask = (1u << kSiteBits) - 1u;
  static constexpr std::uint32_t kNoList = 0u;
  // (site, list) objects: 0b111 above a 6-bit site, a 4-bit n and the list's main-RAM word; no guest address
  // reaches 0xE0000000, so they never meet an object a scope names.
  static constexpr std::uint32_t kListObject = 0xE0000000u;
  static constexpr std::uint32_t kListSiteShift = 23u;
  static constexpr std::uint32_t kListSiteMask = 0x3Fu;
  static constexpr std::uint32_t kListNthShift = 19u;
  static constexpr std::uint32_t kListNthMask = 0xFu;
  static constexpr std::uint32_t kRamMask = 0x1FFFFCu;

  struct Count {
    std::uint32_t site = 0;
    std::uint32_t list = 0;
    std::uint32_t calls = 0;
  };

  // Calls from `site` drawing `list` before this one in the logic frame.
  std::uint32_t count(std::uint32_t site, std::uint32_t list) {
    const auto found = std::find_if(counts_.begin(), counts_.end(), [&](const Count &entry) {
      return entry.site == site && entry.list == list;
    });
    if (found == counts_.end()) {
      counts_.push_back({site, list, 1u});
      return 0u;
    }
    return found->calls++;
  }
  std::uint32_t siteOrdinal(std::uint32_t returnAddress) {
    const auto found = std::find(sites_.begin(), sites_.end(), returnAddress);
    if (found != sites_.end()) {
      return static_cast<std::uint32_t>(found - sites_.begin());
    }
    sites_.push_back(returnAddress);
    return static_cast<std::uint32_t>(sites_.size() - 1u);
  }

  std::vector<Count> counts_;
  std::vector<std::uint32_t> sites_; // sites of list objects, in first-call order
};

} // namespace tomba::render
