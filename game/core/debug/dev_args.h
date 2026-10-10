#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace tomba {

// Words and strict decimal numbers of one control-channel line.
class DevArgs {
public:
  // The whitespace-separated words of `line`, command name first.
  static std::vector<std::string_view> words(std::string_view line);
  // True when `word` is only decimal digits and fits in 32 bits.
  static bool decimal(std::string_view word, uint32_t &value);
};

} // namespace tomba
